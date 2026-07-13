#include "tun_device.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

constexpr const char* kIp = "/sbin/ip";

void run_or_throw(const std::string& cmd) {
  const int rc = ::system(cmd.c_str());
  if (rc != 0) {
    throw std::runtime_error("command failed (" + std::to_string(rc) + "): " + cmd);
  }
}

void run_ignore(const std::string& cmd) { (void)::system(cmd.c_str()); }

}  // namespace

TunDevice::~TunDevice() {
  gateway_.reset();
  if (!netns_.empty()) {
    run_ignore(std::string(kIp) + " netns delete " + netns_ + " 2>/dev/null");
  }
  close();
}

void TunDevice::open(const std::string& iface_name) {
  close();
  fd_ = ::open("/dev/net/tun", O_RDWR);
  if (fd_ < 0) {
    throw std::runtime_error(std::string("open /dev/net/tun: ") + std::strerror(errno));
  }

  ifreq ifr{};
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (iface_name.size() >= IFNAMSIZ) {
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error("iface name too long");
  }
  std::strncpy(ifr.ifr_name, iface_name.c_str(), IFNAMSIZ - 1);

  if (::ioctl(fd_, TUNSETIFF, &ifr) < 0) {
    const auto err = std::string("TUNSETIFF: ") + std::strerror(errno);
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(err);
  }
  name_ = ifr.ifr_name;
}

void TunDevice::configure_lan(const std::string& local_ip) {
  if (fd_ < 0 || name_.empty()) {
    throw std::runtime_error("TUN not open");
  }
  gateway_.reset();
  run_or_throw(std::string(kIp) + " link set dev " + name_ + " up");
  run_or_throw(std::string(kIp) + " addr flush dev " + name_);
  run_or_throw(std::string(kIp) + " addr add " + local_ip + "/24 dev " + name_);
  run_or_throw(std::string(kIp) + " route replace 10.64.0.0/24 dev " + name_);
}

void TunDevice::isolate_in_netns(const std::string& netns, const std::string& local_ip,
                                 int local_port, const std::vector<int>& expose_ports) {
  if (fd_ < 0 || name_.empty()) {
    throw std::runtime_error("TUN not open");
  }
  gateway_.reset();
  run_ignore(std::string(kIp) + " netns delete " + netns + " 2>/dev/null");
  run_or_throw(std::string(kIp) + " netns add " + netns);
  run_or_throw(std::string(kIp) + " link set dev " + name_ + " netns " + netns);
  run_or_throw(std::string(kIp) + " netns exec " + netns + " " + kIp + " link set dev " + name_ +
               " up");
  run_or_throw(std::string(kIp) + " netns exec " + netns + " " + kIp + " addr flush dev " + name_);
  run_or_throw(std::string(kIp) + " netns exec " + netns + " " + kIp + " addr add " + local_ip +
               "/24 dev " + name_);
  run_or_throw(std::string(kIp) + " netns exec " + netns + " " + kIp +
               " route replace 10.64.0.0/24 dev " + name_);
  netns_ = netns;
  gateway_ = std::make_unique<HostGateway>();
  gateway_->install(local_port, netns, expose_ports);
}

void TunDevice::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  name_.clear();
}

void TunDevice::interrupt() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

std::vector<uint8_t> TunDevice::read_packet() {
  std::vector<uint8_t> buf(65535);
  const ssize_t n = ::read(fd_, buf.data(), buf.size());
  if (n <= 0) {
    return {};
  }
  buf.resize(static_cast<size_t>(n));
  return buf;
}

void TunDevice::write_packet(const uint8_t* data, size_t len) {
  if (fd_ < 0 || !data || len == 0) {
    return;
  }
  ::write(fd_, data, len);
}
