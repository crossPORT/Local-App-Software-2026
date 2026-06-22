#include "fabric_session_message.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace {

constexpr char kSessionHeader[] = "FABRIC-SESSION-v1\n";

std::string trim(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) {
        value.pop_back();
    }
    size_t start = 0;
    while (start < value.size() && value[start] == ' ') {
        ++start;
    }
    return value.substr(start);
}

bool parse_line(const std::string& line, FabricSessionMessage& out) {
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
        return false;
    }
    const std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));
    if (key == "kind") {
        out.kind = session_kind_from_string(value);
        return true;
    }
    if (key == "from") {
        out.from_name = value;
        return true;
    }
    if (key == "team") {
        out.team = value;
        return true;
    }
    if (key == "session_id") {
        out.session_id = value;
        return true;
    }
    if (key == "to") {
        out.to_name = value;
        return true;
    }
    if (key == "note") {
        out.note = value;
        return true;
    }
    if (key == "payload_type") {
        out.payload_type = value;
        return true;
    }
    if (key == "payload_name") {
        out.payload_name = value;
        return true;
    }
    if (key == "files") {
        out.file_count = static_cast<uint32_t>(std::stoul(value));
        return true;
    }
    if (key == "total_bytes") {
        out.total_bytes = std::stoull(value);
        return true;
    }
    return false;
}

}  // namespace

std::string session_kind_to_string(SessionMessageKind kind) {
    switch (kind) {
        case SessionMessageKind::Offer:
            return "offer";
        case SessionMessageKind::Accept:
            return "accept";
        case SessionMessageKind::Decline:
            return "decline";
        case SessionMessageKind::Ready:
            return "ready";
        case SessionMessageKind::Announce:
            return "announce";
        case SessionMessageKind::Unknown:
            break;
    }
    return "unknown";
}

SessionMessageKind session_kind_from_string(const std::string& value) {
    if (value == "offer") {
        return SessionMessageKind::Offer;
    }
    if (value == "accept") {
        return SessionMessageKind::Accept;
    }
    if (value == "decline") {
        return SessionMessageKind::Decline;
    }
    if (value == "ready") {
        return SessionMessageKind::Ready;
    }
    if (value == "announce") {
        return SessionMessageKind::Announce;
    }
    return SessionMessageKind::Unknown;
}

std::string session_inbox_relative_path(const std::string& session_id) {
    return ".fabric-session/" + session_id + ".msg";
}

bool is_session_inbox_path(const std::string& relative_path) {
    return relative_path.rfind(".fabric-session/", 0) == 0
        && relative_path.size() > 16
        && relative_path.compare(relative_path.size() - 4, 4, ".msg") == 0;
}

std::string write_session_temp_file(const FabricSessionMessage& message) {
    if (message.session_id.empty()) {
        return {};
    }

    char path_template[] = "/tmp/slsfabric-session-XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        return {};
    }

    std::ostringstream payload;
    payload << kSessionHeader;
    payload << "kind=" << session_kind_to_string(message.kind) << '\n';
    payload << "from=" << message.from_name << '\n';
    payload << "team=" << message.team << '\n';
    payload << "session_id=" << message.session_id << '\n';
    if (!message.to_name.empty()) {
        payload << "to=" << message.to_name << '\n';
    }
    if (!message.note.empty()) {
        payload << "note=" << message.note << '\n';
    }
    if (!message.payload_type.empty()) {
        payload << "payload_type=" << message.payload_type << '\n';
    }
    if (!message.payload_name.empty()) {
        payload << "payload_name=" << message.payload_name << '\n';
    }
    if (message.file_count > 0) {
        payload << "files=" << message.file_count << '\n';
    }
    if (message.total_bytes > 0) {
        payload << "total_bytes=" << message.total_bytes << '\n';
    }

    const std::string text = payload.str();
    if (write(fd, text.data(), text.size()) != static_cast<ssize_t>(text.size())) {
        close(fd);
        std::remove(path_template);
        return {};
    }
    close(fd);
    return path_template;
}

bool read_session_file(const std::string& path, FabricSessionMessage& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string payload((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    if (payload.rfind(kSessionHeader, 0) != 0) {
        return false;
    }

    out = FabricSessionMessage{};
    std::istringstream lines(payload.substr(sizeof(kSessionHeader) - 1));
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        parse_line(line, out);
    }

    return out.kind != SessionMessageKind::Unknown && !out.session_id.empty();
}
