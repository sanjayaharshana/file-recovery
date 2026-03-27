#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace recovery {

struct FatUndeleteOptions {
    std::filesystem::path image_path;
    std::filesystem::path output_dir;
};

/// Recover files deleted on a FAT32 volume (raw image of partition or whole disk with
/// partition offset handled by user supplying a partition image).
class FatUndelete {
public:
    explicit FatUndelete(FatUndeleteOptions opts) : options_(std::move(opts)) {}

    /// Returns number of files recovered, or -1 on fatal error (see err).
    int run(std::string& err);

private:
    FatUndeleteOptions options_;

    struct Bpb {
        std::uint16_t bytes_per_sector{};
        std::uint8_t sectors_per_cluster{};
        std::uint16_t reserved_sector_count{};
        std::uint8_t num_fats{};
        std::uint32_t total_sectors{};
        std::uint32_t sectors_per_fat{};
        std::uint32_t root_cluster{};
        std::uint32_t first_data_sector{};
        std::uint32_t cluster_count{};
    };

    static bool parse_bpb(const std::vector<std::uint8_t>& boot, Bpb& out, std::string& err);
    static std::uint16_t u16_le(const std::uint8_t* p);
    static std::uint32_t u32_le(const std::uint8_t* p);

    bool read_image_at(std::uint64_t offset, void* buf, std::size_t len, std::string& err) const;

    std::vector<std::uint32_t> fat_cache_;
    Bpb bpb_{};

    bool load_fat(const Bpb& bpb, std::string& err);
    std::uint32_t next_cluster(std::uint32_t c) const;

    std::uint64_t cluster_byte_offset(std::uint32_t cluster) const;

    bool read_cluster_chain(std::uint32_t start_cluster, std::uint32_t file_size,
                            std::vector<std::uint8_t>& out, std::string& err) const;

    static bool is_lfn_entry(const std::uint8_t* e);
    static bool is_volume_or_label(std::uint8_t attr);
    static std::string short_name_from_entry(const std::uint8_t* e);
    static std::string sanitize_filename(std::string name);

    bool read_directory_chain(std::uint32_t start_cluster, std::vector<std::uint8_t>& out,
                              std::string& err) const;

    /// parent_deleted: true if this directory is reached under a deleted folder — recover
    /// contained files even when their dirent is not marked 0xE5 (common after folder delete).
    bool walk_dir(std::uint32_t dir_cluster, const std::string& rel_prefix, int& file_counter,
                  std::string& err, bool parent_deleted);

    mutable std::ifstream image_;
};

} // namespace recovery
