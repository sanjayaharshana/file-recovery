#include "carver.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>

namespace recovery {

namespace {

bool match_at(std::span<const std::uint8_t> data, std::size_t pos,
              std::initializer_list<std::uint8_t> sig) {
    if (pos + sig.size() > data.size()) {
        return false;
    }
    std::size_t i = 0;
    for (std::uint8_t b : sig) {
        if (data[pos + i++] != b) {
            return false;
        }
    }
    return true;
}

bool starts_with_pdf(std::span<const std::uint8_t> data, std::size_t pos) {
    static const std::uint8_t sig[] = {0x25, 0x50, 0x44, 0x46}; // %PDF
    return match_at(data, pos, {sig[0], sig[1], sig[2], sig[3]});
}

bool starts_with_zip(std::span<const std::uint8_t> data, std::size_t pos) {
    return match_at(data, pos, {0x50, 0x4B, 0x03, 0x04});
}

bool starts_with_gif(std::span<const std::uint8_t> data, std::size_t pos) {
    if (pos + 6 > data.size()) {
        return false;
    }
    return data[pos] == 0x47 && data[pos + 1] == 0x49 && data[pos + 2] == 0x46 &&
           data[pos + 3] == 0x38 && (data[pos + 4] == 0x37 || data[pos + 4] == 0x39) &&
           data[pos + 5] == 0x61;
}

} // namespace

bool FileCarver::filter_allows(const std::vector<FileKind>& filter, FileKind k) {
    if (filter.empty()) {
        return true;
    }
    return std::find(filter.begin(), filter.end(), k) != filter.end();
}

std::string_view FileCarver::kind_name(FileKind k) {
    switch (k) {
    case FileKind::Jpeg:
        return "jpeg";
    case FileKind::Png:
        return "png";
    case FileKind::Pdf:
        return "pdf";
    case FileKind::Zip:
        return "zip";
    case FileKind::Gif:
        return "gif";
    }
    return "unknown";
}

std::string FileCarver::make_output_name(FileKind k, std::size_t index, std::size_t offset) {
    std::string ext;
    switch (k) {
    case FileKind::Jpeg:
        ext = ".jpg";
        break;
    case FileKind::Png:
        ext = ".png";
        break;
    case FileKind::Pdf:
        ext = ".pdf";
        break;
    case FileKind::Zip:
        ext = ".zip";
        break;
    case FileKind::Gif:
        ext = ".gif";
        break;
    }
    return "recovered_" + std::to_string(index) + "_off" + std::to_string(offset) + ext;
}

std::vector<std::uint8_t> FileCarver::read_all(const std::filesystem::path& path, std::string& err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "Cannot open input: " + path.string();
        return {};
    }
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    if (sz < 0) {
        err = "Cannot determine file size: " + path.string();
        return {};
    }
    const auto usize = static_cast<std::size_t>(sz);
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(usize);
    if (usize > 0) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(usize));
        if (!in) {
            err = "Failed to read full input: " + path.string();
            return {};
        }
    }
    return buf;
}

std::optional<std::size_t> FileCarver::find_jpeg_end(std::span<const std::uint8_t> data,
                                                     std::size_t start) {
    // JPEG ends with 0xFF 0xD9; avoid matching padding 0xFF before D9 in entropy-coded data
    for (std::size_t i = start + 2; i + 1 < data.size(); ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD9) {
            return i + 2;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FileCarver::find_png_end(std::span<const std::uint8_t> data,
                                                    std::size_t start) {
    // IEND chunk: length 0, "IEND", CRC (12 bytes total for chunk structure after length+type)
    static const std::uint8_t iend[] = {0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
                                        0xAE, 0x42, 0x60, 0x82};
    for (std::size_t i = start; i + sizeof(iend) <= data.size(); ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < sizeof(iend); ++j) {
            if (data[i + j] != iend[j]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return i + sizeof(iend);
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FileCarver::find_pdf_end(std::span<const std::uint8_t> data,
                                                    std::size_t start, std::size_t max_len) {
    const std::size_t end_search = std::min(data.size(), start + max_len);
    const std::uint8_t eof_mark[] = {0x25, 0x25, 0x45, 0x4F, 0x46}; // %%EOF
    for (std::size_t i = start; i + sizeof(eof_mark) <= end_search; ++i) {
        if (match_at(data, i, {0x25, 0x25, 0x45, 0x4F, 0x46})) {
            std::size_t end = i + sizeof(eof_mark);
            if (end < data.size() && data[end] == 0x0D) {
                ++end;
            }
            if (end < data.size() && data[end] == 0x0A) {
                ++end;
            }
            return end;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FileCarver::find_zip_end_bounded(std::span<const std::uint8_t> data,
                                                            std::size_t start,
                                                            std::size_t max_len) {
    // EOCD: signature 0x06054b50 at most 65535+N bytes from end — approximate: search for PK\x05\x06
    const std::size_t lim = std::min(data.size(), start + max_len);
    for (std::size_t i = start; i + 22 <= lim; ++i) {
        if (data[i] == 0x50 && data[i + 1] == 0x4B && data[i + 2] == 0x05 && data[i + 3] == 0x06) {
            // EOCD minimum 22 bytes; comment length at offset 20 from i
            if (i + 22 > data.size()) {
                continue;
            }
            const std::uint16_t clen = static_cast<std::uint16_t>(data[i + 20]) |
                                       (static_cast<std::uint16_t>(data[i + 21]) << 8);
            const std::size_t eocd_total = 22u + clen;
            if (i + eocd_total <= data.size() && i + eocd_total <= lim) {
                return i + eocd_total;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FileCarver::find_gif_end(std::span<const std::uint8_t> data,
                                                    std::size_t start, std::size_t max_len) {
    const std::size_t lim = std::min(data.size(), start + max_len);
    for (std::size_t i = start + 6; i + 1 < lim; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x3B) {
            return i + 2;
        }
    }
    return std::nullopt;
}

void FileCarver::scan_signatures(std::span<const std::uint8_t> data,
                                 const std::vector<FileKind>& filter, std::size_t max_unbounded,
                                 std::vector<CarveResult>& out_hits) {
    for (std::size_t pos = 0; pos < data.size(); ++pos) {
        if (match_at(data, pos, {0xFF, 0xD8, 0xFF}) && filter_allows(filter, FileKind::Jpeg)) {
            CarveResult r{.kind = FileKind::Jpeg, .offset_start = pos, .offset_end = 0};
            if (auto end = find_jpeg_end(data, pos)) {
                r.offset_end = *end;
                out_hits.push_back(r);
                pos = r.offset_end - 1;
            }
            continue;
        }
        if (match_at(data, pos,
                     {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}) &&
            filter_allows(filter, FileKind::Png)) {
            CarveResult r{.kind = FileKind::Png, .offset_start = pos, .offset_end = 0};
            if (auto end = find_png_end(data, pos)) {
                r.offset_end = *end;
                out_hits.push_back(r);
                pos = r.offset_end - 1;
            }
            continue;
        }
        if (starts_with_pdf(data, pos) && filter_allows(filter, FileKind::Pdf)) {
            CarveResult r{.kind = FileKind::Pdf, .offset_start = pos, .offset_end = 0};
            if (auto end = find_pdf_end(data, pos, max_unbounded)) {
                r.offset_end = *end;
            } else {
                r.offset_end = std::min(data.size(), pos + max_unbounded);
            }
            if (r.size() > 0) {
                out_hits.push_back(r);
                pos = r.offset_end - 1;
            }
            continue;
        }
        if (starts_with_zip(data, pos) && filter_allows(filter, FileKind::Zip)) {
            CarveResult r{.kind = FileKind::Zip, .offset_start = pos, .offset_end = 0};
            if (auto end = find_zip_end_bounded(data, pos, max_unbounded)) {
                r.offset_end = *end;
            } else {
                r.offset_end = std::min(data.size(), pos + max_unbounded);
            }
            if (r.size() > 0) {
                out_hits.push_back(r);
                pos = r.offset_end - 1;
            }
            continue;
        }
        if (starts_with_gif(data, pos) && filter_allows(filter, FileKind::Gif)) {
            CarveResult r{.kind = FileKind::Gif, .offset_start = pos, .offset_end = 0};
            if (auto end = find_gif_end(data, pos, max_unbounded)) {
                r.offset_end = *end;
            } else {
                r.offset_end = std::min(data.size(), pos + max_unbounded);
            }
            if (r.size() > 0) {
                out_hits.push_back(r);
                pos = r.offset_end - 1;
            }
        }
    }
}

bool FileCarver::extract_range(const std::vector<std::uint8_t>& data, const CarveResult& hit,
                               const std::filesystem::path& out_path, std::string& err) const {
    if (hit.offset_end <= hit.offset_start || hit.offset_end > data.size()) {
        err = "Invalid carve range";
        return false;
    }
    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        err = "Cannot write: " + out_path.string();
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data() + hit.offset_start),
              static_cast<std::streamsize>(hit.size()));
    if (!out) {
        err = "Write failed: " + out_path.string();
        return false;
    }
    return true;
}

int FileCarver::run(std::string& error_message) {
    std::error_code ec;
    std::filesystem::create_directories(options.output_dir, ec);
    if (ec) {
        error_message = "Cannot create output directory: " + options.output_dir.string() + " — " +
                        ec.message();
        return -1;
    }

    std::string read_err;
    std::vector<std::uint8_t> data = read_all(options.input_path, read_err);
    if (!read_err.empty()) {
        error_message = std::move(read_err);
        return -1;
    }

    std::vector<CarveResult> hits;
    scan_signatures(data, options.filter, options.max_unbounded_size, hits);

    int written = 0;
    std::size_t idx = 0;
    for (const auto& h : hits) {
        ++idx;
        const auto name = make_output_name(h.kind, idx, h.offset_start);
        const auto out_path = options.output_dir / name;
        std::string werr;
        if (extract_range(data, h, out_path, werr)) {
            ++written;
        } else {
            error_message = werr;
            return -1;
        }
    }

    error_message.clear();
    return written;
}

} // namespace recovery
