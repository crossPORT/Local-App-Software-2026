#include "fabric_tar_pack.h"

#include "platform_util.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

struct TarMemberEntry {
    std::string base_dir;
    std::string member;
    std::string source_path;
};

std::string temp_tar_path() {
    return platform::create_empty_temp_file("slsfabric-send-", ".tar");
}

bool tar_member_for_file(const std::filesystem::path& file_path,
                         const std::string& source_root,
                         TarMemberEntry* out,
                         std::string* error_out) {
    if (!std::filesystem::is_regular_file(file_path)) {
        if (error_out) {
            *error_out = "Source file not found: " + file_path.string();
        }
        return false;
    }

    std::string base_dir;
    std::string member;

    if (!source_root.empty()) {
        std::string root = source_root;
        const std::string file = file_path.string();
        if (root.back() != '/' && root.back() != '\\') {
            root += '/';
        }
        if (file.compare(0, root.size(), root) == 0) {
            base_dir = source_root;
            member = file.substr(root.size());
        }
    }

    if (member.empty()) {
        base_dir = file_path.parent_path().empty() ? "." : file_path.parent_path().string();
        member = file_path.filename().string();
    }

    if (member.empty() || member.find("..") != std::string::npos) {
        if (error_out) {
            *error_out = "Invalid tar member path";
        }
        return false;
    }

    out->base_dir = base_dir;
    out->member = member;
    out->source_path = file_path.string();
    return true;
}

bool tar_member_for_directory(const std::filesystem::path& directory_path,
                            TarMemberEntry* out,
                            std::string* error_out) {
    if (!std::filesystem::is_directory(directory_path)) {
        if (error_out) {
            *error_out = "Source directory not found: " + directory_path.string();
        }
        return false;
    }

    const std::string base_dir =
        directory_path.parent_path().empty() ? "." : directory_path.parent_path().string();
    const std::string member = directory_path.filename().string();
    if (member.empty() || member == "." || member == "..") {
        if (error_out) {
            *error_out = "Invalid directory name";
        }
        return false;
    }

    out->base_dir = base_dir;
    out->member = member;
    out->source_path = directory_path.string();
    return true;
}

void disambiguate_tar_members(std::vector<TarMemberEntry>& entries) {
    std::map<std::string, int> basename_counts;
    for (const TarMemberEntry& entry : entries) {
        basename_counts[entry.member]++;
    }

    for (TarMemberEntry& entry : entries) {
        if (basename_counts[entry.member] <= 1) {
            continue;
        }
        const std::filesystem::path source(entry.source_path);
        const std::filesystem::path parent = source.parent_path();
        const std::string parent_name = parent.filename().string();
        if (parent_name.empty() || parent_name == "." || parent_name == "..") {
            continue;
        }
        entry.member = parent_name + "/" + entry.member;
        entry.base_dir = parent.parent_path().empty() ? "." : parent.parent_path().string();
    }

    std::set<std::string> used;
    for (TarMemberEntry& entry : entries) {
        if (used.insert(entry.member).second) {
            continue;
        }
        const std::filesystem::path source(entry.source_path);
        const std::string leaf = source.filename().string();
        for (int suffix = 2;; ++suffix) {
            const std::string prefixed = std::to_string(suffix) + "_" + leaf;
            if (used.insert(prefixed).second) {
                entry.member = prefixed;
                entry.base_dir =
                    source.parent_path().empty() ? "." : source.parent_path().string();
                break;
            }
        }
    }
}

std::string create_tar_from_members(const std::vector<TarMemberEntry>& members,
                                    std::string* error_out) {
    if (members.empty()) {
        if (error_out) {
            *error_out = "No tar members";
        }
        return {};
    }

    const std::string tar_path = temp_tar_path();
    if (tar_path.empty()) {
        if (error_out) {
            *error_out = "Could not create temp tar path";
        }
        return {};
    }

    std::vector<std::string> args = {"tar", "cf", tar_path};
    for (const TarMemberEntry& member : members) {
        args.push_back("-C");
        args.push_back(member.base_dir);
        args.push_back(member.member);
    }

    if (!platform::run_command(args, error_out)) {
        std::remove(tar_path.c_str());
        return {};
    }

    return tar_path;
}

}  // namespace

std::string create_tar_for_file(const std::string& file_path,
                                const std::string& source_root,
                                std::string* error_out) {
    TarMemberEntry member;
    if (!tar_member_for_file(std::filesystem::path(file_path), source_root, &member,
                             error_out)) {
        return {};
    }
    return create_tar_from_members({member}, error_out);
}

std::string create_tar_for_directory(const std::string& directory_path,
                                     std::string* error_out) {
    TarMemberEntry member;
    if (!tar_member_for_directory(std::filesystem::path(directory_path), &member, error_out)) {
        return {};
    }
    return create_tar_from_members({member}, error_out);
}

std::string create_tar_for_paths(const std::vector<std::string>& paths,
                                 std::string* error_out) {
    if (paths.empty()) {
        if (error_out) {
            *error_out = "No paths to tar";
        }
        return {};
    }

    if (paths.size() == 1) {
        const std::filesystem::path path(paths[0]);
        if (std::filesystem::is_regular_file(path)) {
            return create_tar_for_file(paths[0], "", error_out);
        }
        if (std::filesystem::is_directory(path)) {
            return create_tar_for_directory(paths[0], error_out);
        }
        if (error_out) {
            *error_out = "Path is not a file or directory: " + paths[0];
        }
        return {};
    }

    std::vector<TarMemberEntry> members;
    members.reserve(paths.size());
    for (const std::string& path : paths) {
        const std::filesystem::path fs_path(path);
        TarMemberEntry member;
        if (std::filesystem::is_regular_file(fs_path)) {
            if (!tar_member_for_file(fs_path, "", &member, error_out)) {
                return {};
            }
        } else if (std::filesystem::is_directory(fs_path)) {
            if (!tar_member_for_directory(fs_path, &member, error_out)) {
                return {};
            }
        } else {
            if (error_out) {
                *error_out = "Path is not a file or directory: " + path;
            }
            return {};
        }
        members.push_back(std::move(member));
    }

    disambiguate_tar_members(members);
    return create_tar_from_members(members, error_out);
}

bool extract_tar_to_dir(const std::string& tar_path,
                        const std::string& target_dir,
                        std::string* error_out) {
    if (tar_path.empty() || target_dir.empty()) {
        if (error_out) {
            *error_out = "Missing tar path or target directory";
        }
        return false;
    }
    if (!std::filesystem::is_regular_file(tar_path)) {
        if (error_out) {
            *error_out = "Tar file not found: " + tar_path;
        }
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(target_dir, ec);
    if (ec) {
        if (error_out) {
            *error_out = "Could not create target directory: " + target_dir;
        }
        return false;
    }

    return platform::run_command({"tar", "xf", tar_path, "-C", target_dir}, error_out);
}
