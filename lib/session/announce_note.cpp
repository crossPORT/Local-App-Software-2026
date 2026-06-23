#include "announce_note.h"

#include <cctype>
#include <random>

namespace {

constexpr int kFabricLegCount = 4;

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

int leg_from_wire_port(int wire_port) {
    if (wire_port >= 1 && wire_port <= kFabricLegCount) {
        return wire_port - 1;
    }
    if (wire_port >= 0 && wire_port < kFabricLegCount) {
        return wire_port;
    }
    return -1;
}

}  // namespace

std::string make_instance_id() {
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string out;
    out.reserve(12);
    for (int i = 0; i < 12; ++i) {
        out.push_back(hex[dist(gen)]);
    }
    return out;
}

std::string build_announce_note(int port_index,
                                ReceiveStatus receive_status,
                                const std::string& instance_id) {
    const int wire_port = port_index + 1;
    std::string note = "port=" + std::to_string(wire_port)
                       + ";receive=" + receive_status_to_string(receive_status);
    if (!instance_id.empty()) {
        note += ";instance=" + instance_id;
    }
    return note;
}

bool parse_announce_note(const std::string& note,
                         int default_port,
                         int* port_index_out,
                         ReceiveStatus* receive_status_out,
                         std::string* instance_id_out) {
    if (!port_index_out || !receive_status_out) {
        return false;
    }
    *port_index_out = default_port;
    *receive_status_out = ReceiveStatus::AskFirst;
    if (instance_id_out) {
        instance_id_out->clear();
    }

    std::string current;
    for (char ch : note) {
        if (ch == ';') {
            std::string value;
            if (parse_key_value(current, "port", &value)) {
                try {
                    const int leg = leg_from_wire_port(std::stoi(value));
                    if (leg >= 0) {
                        *port_index_out = leg;
                    }
                } catch (...) {
                    /* keep default */
                }
            } else if (parse_key_value(current, "receive", &value)) {
                *receive_status_out = receive_status_from_string(value);
            } else if (instance_id_out && parse_key_value(current, "instance", &value)) {
                *instance_id_out = value;
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
                const int leg = leg_from_wire_port(std::stoi(value));
                if (leg >= 0) {
                    *port_index_out = leg;
                }
            } catch (...) {
                /* keep default */
            }
        } else if (parse_key_value(current, "receive", &value)) {
            *receive_status_out = receive_status_from_string(value);
        } else if (instance_id_out && parse_key_value(current, "instance", &value)) {
            *instance_id_out = value;
        }
    }
    return true;
}

int resolve_remote_fabric_port(int my_port, int announced_port) {
    if (announced_port == my_port) {
        return -1;
    }
    if (announced_port < 0 || announced_port >= kFabricLegCount) {
        return -1;
    }
    return announced_port;
}
