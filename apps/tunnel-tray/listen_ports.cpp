#include "listen_ports.hpp"

#include "socket_owners.hpp"

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace tunnel_tray {
namespace {

void scan_proc(const char* path, bool tcp,
               std::map<Endpoint, unsigned long>& inode_by_ep) {
  std::ifstream in(path);
  if (!in) return;
  std::string line;
  std::getline(in, line);  // header
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    // sl local rem st txq:rxq tr:when retrnsmt uid timeout inode …
    std::string sl, local, rem, state, queues, timer, retr, uid, timeout, inode_s;
    ss >> sl >> local >> rem >> state >> queues >> timer >> retr >> uid >> timeout >> inode_s;
    if (local.size() < 9) continue;
    const auto colon = local.find_last_of(':');
    if (colon == std::string::npos) continue;
    int port = 0;
    try {
      port = std::stoi(local.substr(colon + 1), nullptr, 16);
    } catch (...) {
      continue;
    }
    if (port <= 0 || port > 65535) continue;
    if (tcp && state != "0A") continue;
    unsigned long inode = 0;
    try {
      inode = std::stoul(inode_s);
    } catch (...) {
      continue;
    }
    const Endpoint ep{tcp ? Proto::Tcp : Proto::Udp, port};
    if (inode_by_ep.find(ep) == inode_by_ep.end()) inode_by_ep[ep] = inode;
  }
}

const char* well_known(const Endpoint& ep) {
  if (ep.proto == Proto::Tcp) {
    switch (ep.port) {
      case 22:
        return "SSH";
      case 80:
        return "HTTP";
      case 443:
        return "HTTPS";
      case 445:
        return "SMB";
      case 139:
        return "NetBIOS";
      case 3389:
        return "RDP";
      case 5900:
        return "VNC";
      case 8080:
        return "HTTP";
      default:
        return nullptr;
    }
  }
  switch (ep.port) {
    case 53:
      return "DNS";
    case 67:
    case 68:
      return "DHCP";
    case 137:
    case 138:
      return "NetBIOS";
    case 5353:
      return "mDNS";
    default:
      return nullptr;
  }
}

}  // namespace

std::vector<ListeningService> scan_listening_services() {
  std::map<Endpoint, unsigned long> inode_by_ep;
  scan_proc("/proc/net/tcp", true, inode_by_ep);
  scan_proc("/proc/net/tcp6", true, inode_by_ep);
  scan_proc("/proc/net/udp", false, inode_by_ep);
  scan_proc("/proc/net/udp6", false, inode_by_ep);
  const auto owners = socket_inode_to_process();
  std::vector<ListeningService> out;
  out.reserve(inode_by_ep.size());
  for (const auto& kv : inode_by_ep) {
    ListeningService s;
    s.ep = kv.first;
    const auto it = owners.find(kv.second);
    if (it != owners.end()) s.process = it->second;
    out.push_back(std::move(s));
  }
  return out;
}

std::vector<Endpoint> scan_listening_endpoints() {
  std::vector<Endpoint> out;
  for (const auto& s : scan_listening_services()) out.push_back(s.ep);
  return out;
}

std::string service_label(const Endpoint& ep, const std::string& process) {
  const char* name = well_known(ep);
  const char* proto = ep.proto == Proto::Tcp ? "TCP" : "UDP";
  std::string s = name ? std::string(name) + " - " + proto + " " + std::to_string(ep.port)
                       : std::string(proto) + " " + std::to_string(ep.port);
  if (!process.empty()) s += " (" + process + ")";
  return s;
}

std::string endpoint_menu_label(const Endpoint& ep, bool down, const std::string& process) {
  auto s = service_label(ep, down ? std::string{} : process);
  if (down) s += "  (down)";
  return s;
}

std::string endpoint_token(const Endpoint& ep) {
  return std::string(ep.proto == Proto::Tcp ? "tcp:" : "udp:") + std::to_string(ep.port);
}

bool parse_endpoint_token(const std::string& tok, Endpoint& out) {
  if (tok.compare(0, 4, "tcp:") == 0) {
    out.proto = Proto::Tcp;
    out.port = std::stoi(tok.substr(4));
  } else if (tok.compare(0, 4, "udp:") == 0) {
    out.proto = Proto::Udp;
    out.port = std::stoi(tok.substr(4));
  } else {
    return false;
  }
  return out.port > 0 && out.port <= 65535;
}

}  // namespace tunnel_tray
