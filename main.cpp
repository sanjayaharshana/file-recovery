#include "carver.hpp"
#include "fat_undelete.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

bool should_skip_scan_dir_name(const std::string& name) {
    // macOS / Windows metadata & index folders — often EPERM for Terminal/GUI-spawned tools.
    static const char* kSkip[] = {".Spotlight-V100",
                                  ".Trashes",
                                  ".fseventsd",
                                  ".DocumentRevisions-V100",
                                  ".TemporaryItems",
                                  ".PKInstallSandboxManager",
                                  "System Volume Information",
                                  "$RECYCLE.BIN"};
    for (const char* s : kSkip) {
        if (name == s) {
            return true;
        }
    }
    return false;
}

/// Manual recursion with error_code iteration (no throws from directory walking). Skips protected
/// subtrees; subtree errors print a warning and continue.
void carve_walk_directory(const std::filesystem::path& dir, const std::filesystem::path& input_root,
                          const std::filesystem::path& output_root,
                          const recovery::CarveOptions& carve_opts, int& total) {
    namespace fs = std::filesystem;
    std::error_code open_ec;
    const fs::directory_options opts = fs::directory_options::skip_permission_denied;
    fs::directory_iterator it(dir, opts, open_ec);
    if (open_ec) {
        std::cerr << "Warning: cannot open directory " << dir.string() << ": " << open_ec.message()
                  << "\n";
        return;
    }

    const fs::directory_iterator end;
    while (it != end) {
        std::error_code ent_ec;
        const fs::directory_entry& entry = *it;

        const fs::path p = entry.path();
        std::string fname;
        try {
            fname = p.filename().string();
        } catch (const fs::filesystem_error&) {
            it.increment(ent_ec);
            if (ent_ec) {
                break;
            }
            continue;
        }

        const fs::file_status link_st = entry.symlink_status(ent_ec);
        if (ent_ec) {
            it.increment(ent_ec);
            if (ent_ec) {
                break;
            }
            continue;
        }
        if (fs::is_symlink(link_st)) {
            it.increment(ent_ec);
            if (ent_ec) {
                break;
            }
            continue;
        }

        if (fs::is_directory(link_st)) {
            if (!should_skip_scan_dir_name(fname)) {
                try {
                    carve_walk_directory(p, input_root, output_root, carve_opts, total);
                } catch (const fs::filesystem_error& e) {
                    std::cerr << "Warning: skipped subtree " << p.string() << ": " << e.what() << "\n";
                }
            }
            it.increment(ent_ec);
            if (ent_ec) {
                std::cerr << "Warning: listing truncated under " << dir.string() << ": "
                          << ent_ec.message() << "\n";
                break;
            }
            continue;
        }

        if (!fs::is_regular_file(link_st)) {
            it.increment(ent_ec);
            if (ent_ec) {
                break;
            }
            continue;
        }

        fs::path rel;
        try {
            rel = fs::relative(p, input_root);
        } catch (const fs::filesystem_error&) {
            rel = p.filename();
        }

        recovery::CarveOptions one = carve_opts;
        one.input_path = p;
        one.output_dir = output_root / rel.parent_path();
        recovery::FileCarver carver(std::move(one));
        std::string err;
        const int n = carver.run(err);
        if (n < 0) {
            std::cerr << "Warning: " << err << "\n";
        } else {
            total += n;
        }

        it.increment(ent_ec);
        if (ent_ec) {
            std::cerr << "Warning: listing truncated under " << dir.string() << ": "
                      << ent_ec.message() << "\n";
            break;
        }
    }
}

void print_usage(std::string_view prog) {
    std::cerr
        << "Usage: " << prog << " --mode <carve|fat32> --input <path> --output <dir> [options]\n"
        << "  carve: recover files by signature from a file, or scan every file under a folder "
           "(recursive).\n"
        << "  fat32: recover permanently deleted files from a FAT32 volume image (USB/SD).\n\n"
        << "Carve options:\n"
        << "  --types <list>   Comma-separated: jpeg,png,pdf,zip,gif (default: all)\n"
        << "  --max-chunk <n>  Max bytes for formats without a reliable end (default: 52428800)\n"
        << "  -h, --help       Show this help\n";
}

bool parse_types(std::string_view list, std::vector<recovery::FileKind>& out, std::string& err) {
    out.clear();
    std::string buf(list);
    std::istringstream ss(buf);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            continue;
        }
        if (token == "jpeg" || token == "jpg") {
            out.push_back(recovery::FileKind::Jpeg);
        } else if (token == "png") {
            out.push_back(recovery::FileKind::Png);
        } else if (token == "pdf") {
            out.push_back(recovery::FileKind::Pdf);
        } else if (token == "zip") {
            out.push_back(recovery::FileKind::Zip);
        } else if (token == "gif") {
            out.push_back(recovery::FileKind::Gif);
        } else {
            err = "Unknown type: " + token;
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string mode = "carve";
    recovery::CarveOptions carve_opts;
    recovery::FatUndeleteOptions fat_opts;
    std::string types_str;
    bool have_input = false;
    bool have_output = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            const std::filesystem::path p = argv[++i];
            carve_opts.input_path = p;
            fat_opts.image_path = p;
            have_input = true;
        } else if (arg == "--output" && i + 1 < argc) {
            const std::filesystem::path p = argv[++i];
            carve_opts.output_dir = p;
            fat_opts.output_dir = p;
            have_output = true;
        } else if (arg == "--types" && i + 1 < argc) {
            types_str = argv[++i];
        } else if (arg == "--max-chunk" && i + 1 < argc) {
            carve_opts.max_unbounded_size = static_cast<std::size_t>(std::stoull(argv[++i]));
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    if (!have_input || !have_output) {
        print_usage(argv[0]);
        return 2;
    }

    if (mode == "fat32") {
        if (std::filesystem::is_directory(fat_opts.image_path)) {
            std::cerr << "fat32 mode needs a single disk/partition image file, not a folder.\n";
            return 2;
        }
        const std::filesystem::path out_for_msg = fat_opts.output_dir;
        recovery::FatUndelete fat(std::move(fat_opts));
        std::string err;
        const int n = fat.run(err);
        if (n < 0) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "Recovered " << n << " deleted file(s) to " << out_for_msg.string() << "\n";
        return 0;
    }

    if (mode != "carve") {
        std::cerr << "Unknown --mode (use carve or fat32)\n";
        return 2;
    }

    if (!types_str.empty()) {
        std::string perr;
        if (!parse_types(types_str, carve_opts.filter, perr)) {
            std::cerr << perr << "\n";
            return 2;
        }
    }

    const std::filesystem::path input_root = carve_opts.input_path;
    const std::filesystem::path output_root = carve_opts.output_dir;

    if (std::filesystem::is_directory(input_root)) {
        int total = 0;
        carve_walk_directory(input_root, input_root, output_root, carve_opts, total);
        std::cout << "Recovered " << total << " file(s) under " << output_root.string() << "\n";
        return 0;
    }

    recovery::FileCarver carver(std::move(carve_opts));
    std::string err;
    const int n = carver.run(err);
    if (n < 0) {
        std::cerr << err << "\n";
        return 1;
    }

    std::cout << "Recovered " << n << " file(s) to " << carver.options.output_dir.string()
              << "\n";
    return 0;
}
