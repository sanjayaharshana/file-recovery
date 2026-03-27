#include "fat_undelete.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace recovery {

std::uint16_t FatUndelete::u16_le(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t FatUndelete::u32_le(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

bool FatUndelete::parse_bpb(const std::vector<std::uint8_t>& boot, Bpb& out, std::string& err) {
    if (boot.size() < 0x52 + 8) {
        err = "Boot sector too small";
        return false;
    }
    out.bytes_per_sector = u16_le(boot.data() + 0x0B);
    out.sectors_per_cluster = boot[0x0D];
    out.reserved_sector_count = u16_le(boot.data() + 0x0E);
    out.num_fats = boot[0x10];
    const std::uint16_t root_ent = u16_le(boot.data() + 0x11);
    out.total_sectors = u16_le(boot.data() + 0x13);
    if (out.total_sectors == 0) {
        out.total_sectors = u32_le(boot.data() + 0x20);
    }
    out.sectors_per_fat = u16_le(boot.data() + 0x16);
    if (out.sectors_per_fat == 0) {
        out.sectors_per_fat = u32_le(boot.data() + 0x24);
    }
    out.root_cluster = u32_le(boot.data() + 0x2C);

    if (out.bytes_per_sector == 0 || (out.bytes_per_sector % 512) != 0) {
        err = "Invalid bytes per sector";
        return false;
    }
    if (out.sectors_per_cluster == 0 || out.num_fats == 0) {
        err = "Invalid FAT geometry";
        return false;
    }
    if (root_ent != 0) {
        err = "Not a FAT32 volume (root entry count != 0). Use a FAT32 image.";
        return false;
    }
    const char* fs = reinterpret_cast<const char*>(boot.data() + 0x52);
    bool fat32_sig = (std::memcmp(fs, "FAT32   ", 8) == 0);
    if (!fat32_sig && out.sectors_per_fat < 0x100) {
        err = "Volume does not look like FAT32 (missing FAT32 label).";
        return false;
    }

    const std::uint32_t first_data =
        static_cast<std::uint32_t>(out.reserved_sector_count) +
        static_cast<std::uint32_t>(out.num_fats) * out.sectors_per_fat;
    if (out.total_sectors <= first_data) {
        err = "Invalid total sectors vs first data sector";
        return false;
    }
    const std::uint32_t data_sectors = out.total_sectors - first_data;
    out.cluster_count = data_sectors / out.sectors_per_cluster;
    out.first_data_sector = first_data;
    return true;
}

bool FatUndelete::read_image_at(std::uint64_t offset, void* buf, std::size_t len,
                                std::string& err) const {
    image_.seekg(static_cast<std::streamoff>(offset));
    if (!image_) {
        err = "Seek failed on disk image";
        return false;
    }
    image_.read(static_cast<char*>(buf), static_cast<std::streamsize>(len));
    if (static_cast<std::size_t>(image_.gcount()) != len) {
        err = "Short read on disk image";
        return false;
    }
    return true;
}

bool FatUndelete::load_fat(const Bpb& bpb, std::string& err) {
    const std::uint64_t fat_bytes =
        static_cast<std::uint64_t>(bpb.sectors_per_fat) * bpb.bytes_per_sector;
    const std::size_t entries = static_cast<std::size_t>(fat_bytes / 4);
    fat_cache_.assign(entries, 0);
    const std::uint64_t fat_offset =
        static_cast<std::uint64_t>(bpb.reserved_sector_count) * bpb.bytes_per_sector;
    std::vector<std::uint8_t> buf(fat_bytes);
    if (!read_image_at(fat_offset, buf.data(), fat_bytes, err)) {
        return false;
    }
    for (std::size_t i = 0; i < entries && i < fat_cache_.size(); ++i) {
        fat_cache_[i] = u32_le(buf.data() + i * 4) & 0x0FFFFFFFU;
    }
    return true;
}

std::uint32_t FatUndelete::next_cluster(std::uint32_t c) const {
    if (c >= fat_cache_.size()) {
        return 0x0FFFFFF8U;
    }
    return fat_cache_[c];
}

static bool is_eof_mark(std::uint32_t v) {
    return v >= 0x0FFFFFF8U;
}

std::uint64_t FatUndelete::cluster_byte_offset(std::uint32_t cluster) const {
    const std::uint64_t sec =
        static_cast<std::uint64_t>(bpb_.first_data_sector) +
        (static_cast<std::uint64_t>(cluster) - 2ULL) *
            static_cast<std::uint64_t>(bpb_.sectors_per_cluster);
    return sec * static_cast<std::uint64_t>(bpb_.bytes_per_sector);
}

bool FatUndelete::read_cluster_chain(std::uint32_t start_cluster, std::uint32_t file_size,
                                     std::vector<std::uint8_t>& out, std::string& err) const {
    out.clear();
    if (start_cluster < 2) {
        err = "Invalid start cluster";
        return false;
    }
    std::uint32_t remaining = file_size;
    std::uint32_t c = start_cluster;
    const std::uint32_t csize = static_cast<std::uint32_t>(bpb_.sectors_per_cluster) *
                                  static_cast<std::uint32_t>(bpb_.bytes_per_sector);

    while (remaining > 0) {
        if (c < 2 || c >= fat_cache_.size()) {
            err = "FAT chain out of range";
            return false;
        }
        const std::uint64_t off = cluster_byte_offset(c);
        const std::uint32_t chunk = std::min(remaining, csize);
        const std::size_t old = out.size();
        out.resize(old + chunk);
        if (!read_image_at(off, out.data() + old, chunk, err)) {
            return false;
        }
        remaining -= chunk;
        if (remaining == 0) {
            break;
        }
        const std::uint32_t n = next_cluster(c);
        if (is_eof_mark(n)) {
            err = "FAT chain ended early";
            return false;
        }
        c = n;
    }
    return true;
}

bool FatUndelete::read_directory_chain(std::uint32_t start_cluster, std::vector<std::uint8_t>& out,
                                       std::string& err) const {
    out.clear();
    if (start_cluster < 2) {
        err = "Invalid directory cluster";
        return false;
    }
    std::uint32_t c = start_cluster;
    const std::uint32_t csize = static_cast<std::uint32_t>(bpb_.sectors_per_cluster) *
                                static_cast<std::uint32_t>(bpb_.bytes_per_sector);
    while (true) {
        if (c < 2 || c >= fat_cache_.size()) {
            err = "Directory FAT chain out of range";
            return false;
        }
        const std::uint64_t off = cluster_byte_offset(c);
        const std::size_t old = out.size();
        out.resize(old + csize);
        if (!read_image_at(off, out.data() + old, csize, err)) {
            return false;
        }
        const std::uint32_t n = next_cluster(c);
        if (is_eof_mark(n)) {
            break;
        }
        c = n;
    }
    return true;
}

bool FatUndelete::is_lfn_entry(const std::uint8_t* e) {
    return e[11] == 0x0F;
}

bool FatUndelete::is_volume_or_label(std::uint8_t attr) {
    return (attr & 0x08) != 0 && (attr & 0x10) == 0;
}

std::string FatUndelete::short_name_from_entry(const std::uint8_t* e) {
    char base[9] = {};
    for (int i = 0; i < 8; ++i) {
        base[i] = static_cast<char>(e[i]);
    }
    if (static_cast<unsigned char>(base[0]) == 0xE5) {
        base[0] = '_';
    }
    std::string b(base);
    while (!b.empty() && b.back() == ' ') {
        b.pop_back();
    }
    char ext[4] = {};
    for (int i = 0; i < 3; ++i) {
        ext[i] = static_cast<char>(e[8 + i]);
    }
    std::string x(ext);
    while (!x.empty() && x.back() == ' ') {
        x.pop_back();
    }
    if (x.empty()) {
        return b;
    }
    return b + "." + x;
}

std::string FatUndelete::sanitize_filename(std::string name) {
    static const char* bad = "\\/:*?\"<>|";
    for (char& c : name) {
        if (std::strchr(bad, c) != nullptr || static_cast<unsigned char>(c) < 0x20) {
            c = '_';
        }
    }
    if (name.empty() || name == "." || name == "..") {
        return "_unnamed";
    }
    return name;
}

bool FatUndelete::walk_dir(std::uint32_t dir_cluster, const std::string& rel_prefix,
                           int& file_counter, std::string& err, bool parent_deleted) {
    std::vector<std::uint8_t> dir_data;
    if (!read_directory_chain(dir_cluster, dir_data, err)) {
        return false;
    }

    for (std::size_t idx = 0; idx + 32 <= dir_data.size(); idx += 32) {
        const std::uint8_t* e = dir_data.data() + idx;
        if (e[0] == 0) {
            break;
        }
        if (is_lfn_entry(e)) {
            continue;
        }

        const std::uint8_t attr = e[11];
        if (is_volume_or_label(attr)) {
            continue;
        }

        std::string name = short_name_from_entry(e);
        if (name == "." || name == "..") {
            continue;
        }

        const std::uint32_t lo = u16_le(e + 0x1A);
        const std::uint32_t hi = u16_le(e + 0x14);
        const std::uint32_t cluster = (hi << 16) | lo;
        const std::uint32_t fsize = u32_le(e + 0x1C);

        const bool deleted = (e[0] == 0xE5);
        const bool is_dir = (attr & 0x10) != 0;

        const bool inside_deleted_tree = parent_deleted || deleted;

        if (is_dir && cluster >= 2) {
            if (!walk_dir(cluster, rel_prefix + sanitize_filename(name) + "/", file_counter, err,
                          inside_deleted_tree)) {
                return false;
            }
        } else if (!is_dir && cluster >= 2 && fsize > 0 && inside_deleted_tree) {
            std::vector<std::uint8_t> file_data;
            if (!read_cluster_chain(cluster, fsize, file_data, err)) {
                return false;
            }
            std::filesystem::path out_path = options_.output_dir;
            if (!rel_prefix.empty()) {
                out_path /= rel_prefix;
            }
            out_path /= sanitize_filename(name);
            std::error_code ec;
            std::filesystem::create_directories(out_path.parent_path(), ec);
            if (ec) {
                err = ec.message();
                return false;
            }
            std::ofstream out(out_path, std::ios::binary);
            if (!out) {
                err = "Cannot write: " + out_path.string();
                return false;
            }
            out.write(reinterpret_cast<const char*>(file_data.data()),
                      static_cast<std::streamsize>(file_data.size()));
            if (!out) {
                err = "Write failed: " + out_path.string();
                return false;
            }
            ++file_counter;
        }
    }
    return true;
}

int FatUndelete::run(std::string& err) {
    image_.open(options_.image_path, std::ios::binary);
    if (!image_) {
        err = "Cannot open disk image: " + options_.image_path.string();
        return -1;
    }

    std::vector<std::uint8_t> boot(512);
    if (!read_image_at(0, boot.data(), boot.size(), err)) {
        return -1;
    }
    if (!parse_bpb(boot, bpb_, err)) {
        return -1;
    }
    if (!load_fat(bpb_, err)) {
        return -1;
    }

    std::error_code ec;
    std::filesystem::create_directories(options_.output_dir, ec);
    if (ec) {
        err = "Cannot create output directory: " + ec.message();
        return -1;
    }

    int file_counter = 0;
    if (!walk_dir(bpb_.root_cluster, "", file_counter, err, false)) {
        return -1;
    }

    err.clear();
    return file_counter;
}

} // namespace recovery
