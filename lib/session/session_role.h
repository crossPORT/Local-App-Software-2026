#pragma once

#include <string>

enum class SessionRole {
    Receiver,
    Sender,
};

SessionRole session_role_from_string(const std::string& value);
std::string session_role_to_string(SessionRole role);
SessionRole default_session_role_for_port(int port_index);
