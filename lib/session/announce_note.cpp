#include "announce_note.h"

#include <cctype>

namespace {

std::string trim_copy(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    return value.substr(start);
}

bool parse_key_value(const std::string& token, const std::string& key, std::string* value_out) {
    const std::string prefix = key + '=';
    if (token.rfind(prefix, 0) != 0) {
        return false;
    }
    *value_out = trim_copy(token.substr(prefix.size()));
    return true;
}

}  // namespace

std::string build_announce_note(int port_index, ReceiveStatus receive_status) {
    return "port=" + std::to_string(port_index) + ";receive=" + receive_status_to_string(receive_status);
}

bool parse_announce_note(const std::string& note,
                         int default_port,
                         int* port_index_out,
                         ReceiveStatus* receive_status_out) {
    if (!port_index_out || !receive_status_out) {
        return false;
    }
    *port_index_out = default_port;
    *receive_status_out = ReceiveStatus::AskFirst;

    std::string current;
    for (char ch : note) {
        if (ch == ';') {
            std::string value;
            if (parse_key_value(current, "port", &value)) {
                try {
                    *port_index_out = std::stoi(value);
                } catch (...) {
                    /* keep default */
                }
            } else if (parse_key_value(current, "receive", &value)) {
                *receive_status_out = receive_status_from_string(value);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        std::string value;
        if (parse_key_value(current, "port", &value)) {
            try {
                *port_index_out = std::stoi(value);
            } catch (...) {
                /* keep default */
            }
        } else if (parse_key_value(current, "receive", &value)) {
            *receive_status_out = receive_status_from_string(value);
        }
    }
    return true;
}
