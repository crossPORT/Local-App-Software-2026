#include "helper_launch.hpp"

#include "helper/protocol.hpp"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace tunnel_tray {
namespace {

bool file_executable(const std::string& path) {
  struct stat st {};
  return ::stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR);
}

std::string dirname_of(const std::string& path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) return ".";
  if (pos == 0) return "/";
  return path.substr(0, pos);
}

std::string self_dir() {
  char self[4096];
  const ssize_t n = ::readlink("/proc/self/exe", self, sizeof(self) - 1);
  if (n <= 0) return {};
  self[n] = '\0';
  return dirname_of(self);
}

}  // namespace

std::string default_helper_bin() {
  if (const char* env = std::getenv("ROCKETBOX_TUNNEL_HELPER_PATH")) {
    if (file_executable(env)) return env;
  }
  const std::string dir = self_dir();
  if (!dir.empty()) {
    const std::string candidates[] = {
        dir + "/rocketbox-tunnel-helper",
        dirname_of(dir) + "/rocketbox-tunnel/rocketbox-tunnel-helper",
        dirname_of(dirname_of(dir)) + "/rocketbox-tunnel/rocketbox-tunnel-helper",
    };
    for (const auto& path : candidates) {
      if (file_executable(path)) return path;
    }
  }
  return "rocketbox-tunnel-helper";
}

bool helper_responds() {
  std::string reply, err;
  return tunnel_helper::send_command("STATUS", reply, err);
}

bool ensure_helper_elevated(std::string& error) {
  if (helper_responds()) return true;
  const std::string bin = default_helper_bin();
  if (!file_executable(bin) && bin.find('/') != std::string::npos) {
    error = "rocketbox-tunnel-helper not found";
    return false;
  }
  if (::geteuid() == 0) {
    const pid_t child = ::fork();
    if (child == 0) {
      ::setsid();
      ::execl(bin.c_str(), bin.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    if (child < 0) {
      error = "fork helper failed";
      return false;
    }
  } else if (file_executable("/usr/bin/pkexec")) {
    const pid_t child = ::fork();
    if (child == 0) {
      ::setsid();
      ::execl("/usr/bin/pkexec", "pkexec", bin.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    if (child < 0) {
      error = "fork pkexec failed";
      return false;
    }
  } else {
    error = "need root or pkexec to start tunnel helper";
    return false;
  }
  for (int i = 0; i < 300; ++i) {
    if (helper_responds()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  error = "approve the password prompt once to start the tunnel helper";
  return false;
}

}  // namespace tunnel_tray
