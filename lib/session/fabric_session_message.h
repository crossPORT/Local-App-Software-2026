#pragma once

#include <cstdint>
#include <string>

enum class SessionMessageKind {
    Offer,
    Accept,
    Decline,
    Ready,
    Announce,
    Unknown,
};

struct FabricSessionMessage {
    SessionMessageKind kind = SessionMessageKind::Unknown;
    std::string from_name;
    std::string team;
    std::string session_id;
    std::string to_name;
    std::string note;
    std::string payload_type;  // "file" | "tar"
    std::string payload_name;
    uint32_t file_count = 0;
    uint64_t total_bytes = 0;
};

std::string session_kind_to_string(SessionMessageKind kind);
SessionMessageKind session_kind_from_string(const std::string& value);

// Relative path used in USB transfer (session inbox convention).
std::string session_inbox_relative_path(const std::string& session_id);

std::string write_session_temp_file(const FabricSessionMessage& message);
bool read_session_file(const std::string& path, FabricSessionMessage& out);

bool is_session_inbox_path(const std::string& relative_path);
