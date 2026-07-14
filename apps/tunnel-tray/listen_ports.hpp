#pragma once

#include <map>
#include <string>
#include <vector>

namespace tunnel_tray {

enum class Proto { Tcp, Udp };

struct Endpoint {
  Proto proto = Proto::Tcp;
  int port = 0;

  bool operator==(const Endpoint& o) const { return proto == o.proto && port == o.port; }
  bool operator<(const Endpoint& o) const {
    if (proto != o.proto) {
      return static_cast<int>(proto) < static_cast<int>(o.proto);
    }
    return port < o.port;
  }
};

struct ListeningService {
  Endpoint ep;
  std::string process;  // may be empty if unknown / no permission
};

/** TCP LISTEN + UDP bound sockets with owning process names when available. */
std::vector<ListeningService> scan_listening_services();

/** Unique endpoints only (no process names). */
std::vector<Endpoint> scan_listening_endpoints();

/** "SSH - TCP 22 (sshd)" / "TCP 53124 (chrome)" / "… - not listening". */
std::string service_label(const Endpoint& ep, const std::string& process = {});
std::string endpoint_menu_label(const Endpoint& ep, bool down, const std::string& process = {});

std::string endpoint_token(const Endpoint& ep);
bool parse_endpoint_token(const std::string& tok, Endpoint& out);

}  // namespace tunnel_tray
