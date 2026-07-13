#include "tun_device.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <net/if.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <unistd.h>

// UTUN_CONTROL_NAME / UTUN_OPT_IFNAME
#include <net/if_utun.h>

namespace {

void run_or_throw(const std::string& cmd) {
  if (::system(cmd.c_str()) != 0) {
    throw std::runtime_error("command failed: " + cmd);
  }
}

}  // namespace

TunDevice::~TunDevice() { close(); }

void TunDevice::open(const std::string& /*iface_name*/) {
  close();
  fd_ = ::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  if (fd_ < 0) {
    throw std::runtime_error(std::string("utun socket: ") + std::strerror(errno));
  }
  ctl_info ci{};
  std::strncpy(ci.ctl_name, UTUN_CONTROL_NAME, sizeof(ci.ctl_name) - 1);
  if (::ioctl(fd_, CTLIOCGINFO, &ci) < 0) {
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(std::string("CTLIOCGINFO: ") + std::strerror(errno));
  }
  sockaddr_ctl sc{};
  sc.sc_id = ci.ctl_id;
  sc.sc_len = sizeof(sc);
  sc.sc_family = AF_SYSTEM;
  sc.ss_sysaddr = AF_SYS_CONTROL;
  sc.sc_unit = 0;
  if (::connect(fd_, reinterpret_cast<sockaddr*>(&sc), sizeof(sc)) < 0) {
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(std::string("utun connect: ") + std::strerror(errno));
  }
  char ifname[IFNAMSIZ] = {};
  socklen_t ifname_len = sizeof(ifname);
  if (::getsockopt(fd_, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &ifname_len) < 0) {
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(std::string("UTUN_OPT_IFNAME: ") + std::strerror(errno));
  }
  name_ = ifname;
}

void TunDevice::configure_lan(const std::string& local_ip) {
  if (fd_ < 0 || name_.empty()) throw std::runtime_error("TUN not open");
  run_or_throw("ifconfig " + name_ + " inet " + local_ip + " " + local_ip + " netmask 255.255.255.0 up");
  run_or_throw("route -n add -net 10.64.0.0/24 -interface " + name_);
}

void TunDevice::isolate_in_netns(const std::string&, const std::string&, int,
                                 const std::vector<ExposeRule>&) {
  throw std::runtime_error("netns isolation is Linux-only; use --no-netns on macOS");
}

void TunDevice::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  name_.clear();
}

void TunDevice::interrupt() { close(); }

std::vector<uint8_t> TunDevice::read_packet() {
  std::vector<uint8_t> buf(65535);
  const ssize_t n = ::read(fd_, buf.data(), buf.size());
  if (n <= 4) return {};
  // Strip 4-byte AF header (utun).
  return std::vector<uint8_t>(buf.begin() + 4, buf.begin() + n);
}

void TunDevice::write_packet(const uint8_t* data, size_t len) {
  if (fd_ < 0 || !data || len == 0) return;
  std::vector<uint8_t> frame(len + 4);
  const uint32_t af = htonl(AF_INET);
  std::memcpy(frame.data(), &af, 4);
  std::memcpy(frame.data() + 4, data, len);
  ::write(fd_, frame.data(), frame.size());
}
