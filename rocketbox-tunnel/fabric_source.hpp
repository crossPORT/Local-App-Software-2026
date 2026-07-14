#pragma once

#include <cstdint>
#include <vector>

/** If dest is fabric LAN but source is not local fabric IP, rewrite src to 10.64.0.local
 *  and refresh the IPv4 header checksum. ICMP payload checksum is unchanged. */
bool force_fabric_source(std::vector<uint8_t>& pkt, int local_port);
