#include "fabric_tar_pack.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

bool run_command(std::vector<std::string> args, std::string* error_out) {
    if (args.empty()) {
        return false;
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        if (error_out) {
            *error_out = "fork failed";
        }
        return false;
    }
    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        if (error_out) {
            *error_out = "waitpid failed";
        }
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (error_out) {
            *error_out = std::string(args[0]) + " exited with status "
                         + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        }
        return false;
    }
    return true;
}

std::string temp_tar_path() {
    char path_template[] = "/tmp/slsfabric-send-XXXXXX.tar";
    const int fd = mkstemps(path_template, 4);
    if (fd < 0) {
        return {};
    }
    close(fd);
    return path_template;
}

}  // namespace

std::string create_tar_for_file(const std::string& file_path,
                                const std::string& source_root,
                                std::string* error_out) {
    if (file_path.empty() || !std::filesystem::is_regular_file(file_path)) {
        if (error_out) {
            *error_out = "Source file not found: " + file_path;
        }
        return {};
    }

    std::string base_dir;
    std::string member;

    if (!source_root.empty()) {
        std::string root = source_root;
        if (root.back() != '/') {
            root += '/';
        }
        if (file_path.compare(0, root.size(), root) == 0) {
            base_dir = source_root;
            member = file_path.substr(root.size());
        }
    }

    if (member.empty()) {
        const std::filesystem::path path(file_path);
        base_dir = path.parent_path().empty() ? "." : path.parent_path().string();
        member = path.filename().string();
    }

    if (member.empty() || member.find("..") != std::string::npos) {
        if (error_out) {
            *error_out = "Invalid tar member path";
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

    if (!run_command({"tar", "cf", tar_path, "-C", base_dir, member}, error_out)) {
        std::remove(tar_path.c_str());
        return {};
    }

    return tar_path;
}

std::string create_tar_for_directory(const std::string& directory_path,
                                     std::string* error_out) {
    if (directory_path.empty() || !std::filesystem::is_directory(directory_path)) {
        if (error_out) {
            *error_out = "Source directory not found: " + directory_path;
        }
        return {};
    }

    const std::filesystem::path dir(directory_path);
    const std::string base_dir = dir.parent_path().empty() ? "." : dir.parent_path().string();
    const std::string member = dir.filename().string();
    if (member.empty() || member == "." || member == "..") {
        if (error_out) {
            *error_out = "Invalid directory name";
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

    if (!run_command({"tar", "cf", tar_path, "-C", base_dir, member}, error_out)) {
        std::remove(tar_path.c_str());
        return {};
    }

    return tar_path;
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

    return run_command({"tar", "xf", tar_path, "-C", target_dir}, error_out);
}
