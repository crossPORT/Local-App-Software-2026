#pragma once

#include "identity_profile.h"

#include <string>

// Announce note wire format: port=1;receive=open
std::string build_announce_note(int port_index, ReceiveStatus receive_status);
bool parse_announce_note(const std::string& note,
                         int default_port,
                         int* port_index_out,
                         ReceiveStatus* receive_status_out);
