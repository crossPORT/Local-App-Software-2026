#pragma once

#include "identity_profile.h"

#include <string>

// Announce note wire format: port=1;receive=open;instance=abc123
std::string make_instance_id();
int resolve_remote_fabric_port(int my_port, int announced_port);
std::string build_announce_note(int port_index,
                                ReceiveStatus receive_status,
                                const std::string& instance_id = "");
bool parse_announce_note(const std::string& note,
                         int default_port,
                         int* port_index_out,
                         ReceiveStatus* receive_status_out,
                         std::string* instance_id_out = nullptr);
