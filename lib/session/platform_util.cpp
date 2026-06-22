#include "platform_util.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform {
namespace {

std::string random_suffix() {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream out;
    out << std::hex << dist(rng) << dist(rng);
    return out.str();
}

std::filesystem::path temp_file_candidate(const std::string& prefix,
                                          const std::string& suffix) {
    return std::filesystem::temp_directory_path()
        / (prefix + random_suffix() + suffix);
}

void local_time(std::time_t seconds, std::tm* out) {
#ifdef _WIN32
    localtime_s(out, &seconds);
#else
    localtime_r(&seconds, out);
#endif
}

}  // namespace

std::string home_directory() {
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return home;
    }
#ifdef _WIN32
    if (const char* profile = std::getenv("USERPROFILE");
        profile != nullptr && *profile != '\0') {
        return profile;
    }
#endif
    return {};
}

std::string expand_home(std::string path) {
    if (path.size() >= 2 && path[0] == '~' && path[1] == '/') {
        const std::string home = home_directory();
        if (!home.empty()) {
            path.replace(0, 1, home);
        }
    }
    return path;
}

std::string temp_directory() {
    return std::filesystem::temp_directory_path().string();
}

std::string default_booth_log_path() {
    return (std::filesystem::path(temp_directory()) / "slsfabric-booth.log").string();
}

std::string shared_booth_path_config() {
    return (std::filesystem::path(temp_directory()) / "slsfabric-booth-path.conf")
        .string();
}

std::string create_empty_temp_file(const std::string& prefix, const std::string& suffix) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        const std::filesystem::path path = temp_file_candidate(prefix, suffix);
        if (std::filesystem::exists(path)) {
            continue;
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            continue;
        }
        return path.string();
    }
    return {};
}

std::string write_temp_text_file(const std::string& prefix, const std::string& content) {
    const std::string path = create_empty_temp_file(prefix);
    if (path.empty()) {
        return {};
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return {};
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file.good()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return {};
    }
    return path;
}

bool run_command(const std::vector<std::string>& args, std::string* error_out) {
    if (args.empty()) {
        if (error_out) {
            *error_out = "empty command";
        }
        return false;
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

#ifdef _WIN32
    const int status = _spawnvp(_P_WAIT, args[0].c_str(), argv.data());
    if (status != 0) {
        if (error_out) {
            *error_out = args[0] + " exited with status " + std::to_string(status);
        }
        return false;
    }
    return true;
#else
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

    int wait_status = 0;
    if (waitpid(pid, &wait_status, 0) < 0) {
        if (error_out) {
            *error_out = "waitpid failed";
        }
        return false;
    }
    if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
        if (error_out) {
            *error_out = args[0] + " exited with status "
                         + std::to_string(WIFEXITED(wait_status) ? WEXITSTATUS(wait_status)
                                                                 : -1);
        }
        return false;
    }
    return true;
#endif
}

bool stderr_is_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(STDERR_FILENO) != 0;
#endif
}

std::string format_timestamp_iso8601_ms() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;
    std::tm tm_buf{};
    local_time(seconds, &tm_buf);
    std::ostringstream out;
    out << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return out.str();
}

}  // namespace platform
