#include "fabric_meta_file.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace {

constexpr char kMetaHeaderV1[] = "FABRIC-META-v1\n";
constexpr char kMetaHeaderV2[] = "FABRIC-META-v2\n";

std::string sanitize_relative_name(const std::string& raw) {
    if (raw.empty() || raw == "." || raw == "..") {
        return {};
    }
    std::string name = raw;
    if (!name.empty() && name.front() == '/') {
        name.erase(name.begin());
    }
    if (name.find("..") != std::string::npos) {
        return {};
    }
    return name;
}

std::string sanitize_type(const std::string& raw) {
    if (raw == "file" || raw == "tar") {
        return raw;
    }
    return "file";
}

bool parse_v2_line(const std::string& line, FabricSendMeta& out) {
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
        return false;
    }
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "type") {
        out.type = sanitize_type(value);
        return true;
    }
    if (key == "name") {
        out.relative_name = sanitize_relative_name(value);
        return true;
    }
    return false;
}

}  // namespace

bool fabric_meta_is_file(const FabricSendMeta& meta) {
    return meta.type == "file" || meta.type.empty();
}

bool fabric_meta_is_tar_archive(const FabricSendMeta& meta) {
    return meta.type == "tar";
}

std::string write_meta_temp_file(const FabricSendMeta& meta) {
    const std::string safe_name = sanitize_relative_name(meta.relative_name);
    if (safe_name.empty()) {
        return {};
    }

    char path_template[] = "/tmp/slsfabric-meta-XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        return {};
    }

    std::ostringstream payload;
    payload << kMetaHeaderV2;
    payload << "type=" << sanitize_type(meta.type) << '\n';
    payload << "name=" << safe_name << '\n';
    const std::string text = payload.str();

    if (write(fd, text.data(), text.size()) != static_cast<ssize_t>(text.size())) {
        close(fd);
        std::remove(path_template);
        return {};
    }
    close(fd);
    return path_template;
}

bool read_meta_file(const std::string& path, FabricSendMeta& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string payload((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    if (payload.rfind(kMetaHeaderV2, 0) == 0) {
        out = FabricSendMeta{};
        std::istringstream lines(payload.substr(sizeof(kMetaHeaderV2) - 1));
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }
            parse_v2_line(line, out);
        }
        return !out.relative_name.empty();
    }

    if (payload.rfind(kMetaHeaderV1, 0) == 0) {
        const std::size_t name_start = sizeof(kMetaHeaderV1) - 1;
        std::size_t name_end = payload.find('\n', name_start);
        if (name_end == std::string::npos) {
            name_end = payload.size();
        }
        const std::string relative_name =
            sanitize_relative_name(payload.substr(name_start, name_end - name_start));
        if (relative_name.empty()) {
            return false;
        }
        out.type = "file";
        out.relative_name = relative_name;
        return true;
    }

    return false;
}

std::string target_path_for_name(const std::string& target_dir,
                                 const std::string& relative_name) {
    const std::string safe_name = sanitize_relative_name(relative_name);
    if (safe_name.empty() || target_dir.empty()) {
        return {};
    }

    std::string dir = target_dir;
    if (dir.back() != '/') {
        dir += '/';
    }
    return dir + safe_name;
}

bool ensure_parent_directories(const std::string& file_path, std::string* error_out) {
    const std::filesystem::path parent = std::filesystem::path(file_path).parent_path();
    if (parent.empty()) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        if (error_out) {
            *error_out = "Could not create directory: " + parent.string();
        }
        return false;
    }
    return true;
}
