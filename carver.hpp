#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace recovery {

enum class FileKind : std::uint8_t {
    Jpeg,
    Png,
    Pdf,
    Zip,
    Gif,
};

struct CarveResult {
    FileKind kind{};
    std::size_t offset_start{};
    std::size_t offset_end{}; // exclusive
    std::size_t size() const { return offset_end - offset_start; }
};

struct CarveOptions {
    std::filesystem::path input_path;
    std::filesystem::path output_dir;
    /// Max bytes to take when no reliable end marker exists (PDF/ZIP/GIF).
    std::size_t max_unbounded_size = 50 * 1024 * 1024;
    /// If set, only these kinds are extracted; empty means all supported kinds.
    std::vector<FileKind> filter;
};

class FileCarver {
public:
    explicit FileCarver(CarveOptions opts) : options(std::move(opts)) {}

    CarveOptions options;

    /// Scan and write recovered files. Returns count of files written or negative on error.
    int run(std::string& error_message);

private:
    static bool filter_allows(const std::vector<FileKind>& filter, FileKind k);
    static std::string_view kind_name(FileKind k);
    static std::string make_output_name(FileKind k, std::size_t index, std::size_t offset);

    bool extract_range(const std::vector<std::uint8_t>& data, const CarveResult& hit,
                       const std::filesystem::path& out_path, std::string& err) const;

    static std::vector<std::uint8_t> read_all(const std::filesystem::path& path, std::string& err);

    static std::optional<std::size_t> find_jpeg_end(std::span<const std::uint8_t> data,
                                                      std::size_t start);
    static std::optional<std::size_t> find_png_end(std::span<const std::uint8_t> data,
                                                   std::size_t start);
    static std::optional<std::size_t> find_pdf_end(std::span<const std::uint8_t> data,
                                                   std::size_t start, std::size_t max_len);
    static std::optional<std::size_t> find_zip_end_bounded(std::span<const std::uint8_t> data,
                                                           std::size_t start, std::size_t max_len);
    static std::optional<std::size_t> find_gif_end(std::span<const std::uint8_t> data,
                                                   std::size_t start, std::size_t max_len);

    static void scan_signatures(std::span<const std::uint8_t> data,
                                const std::vector<FileKind>& filter, std::size_t max_unbounded,
                                std::vector<CarveResult>& out_hits);
};

} // namespace recovery
