#include "session_role.h"

#include <algorithm>
#include <cctype>

namespace {

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

}  // namespace

SessionRole session_role_from_string(const std::string& value) {
    const std::string key = lower_copy(value);
    if (key == "sender" || key == "send") {
        return SessionRole::Sender;
    }
    return SessionRole::Receiver;
}

std::string session_role_to_string(SessionRole role) {
    return role == SessionRole::Sender ? "sender" : "receiver";
}

SessionRole default_session_role_for_port(int port_index) {
    return port_index == 1 ? SessionRole::Sender : SessionRole::Receiver;
}
