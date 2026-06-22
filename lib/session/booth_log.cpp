#include "booth_log.h"

#include <chrono>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

std::mutex log_mutex;
std::string log_path = "/tmp/slsfabric-booth.log";

std::string timestamp_now() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream out;
    out << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return out.str();
}

void write_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream file(log_path, std::ios::app);
    if (file.is_open()) {
        file << line << '\n';
    }
    if (std::getenv("SLSFABRIC_LOG_STDERR") || isatty(STDERR_FILENO)) {
        std::cerr << line << '\n';
    }
}

}  // namespace

void booth_log_set_path(const std::string& path) {
    std::lock_guard<std::mutex> lock(log_mutex);
    log_path = path;
}

std::string booth_log_path() {
    std::lock_guard<std::mutex> lock(log_mutex);
    return log_path;
}

void booth_log_clear() {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream file(log_path, std::ios::trunc);
    if (file.is_open()) {
        file << timestamp_now() << " [boot] log cleared\n";
    }
}

std::string read_booth_log_tail(std::size_t max_lines) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ifstream file(log_path);
    if (!file.is_open()) {
        return "(log file not found: " + log_path + ")\n";
    }

    std::deque<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(std::move(line));
        if (lines.size() > max_lines) {
            lines.pop_front();
        }
    }

    std::ostringstream out;
    for (const std::string& entry : lines) {
        out << entry << '\n';
    }
    if (lines.empty()) {
        out << "(log is empty)\n";
    }
    return out.str();
}

void booth_log(int port, const std::string& event, const std::string& detail) {
    if (const char* env = std::getenv("SLSFABRIC_LOG")) {
        booth_log_set_path(env);
    }

    std::ostringstream line;
    line << timestamp_now() << " [port=" << port << "] " << event;
    if (!detail.empty()) {
        line << " | " << detail;
    }
    write_line(line.str());
}
