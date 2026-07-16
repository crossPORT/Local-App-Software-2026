#include "helper/commands.hpp"
#include "helper/commands_plat.hpp"

#if !defined(_WIN32)
#include <csignal>
#endif

namespace tunnel_helper {

std::string handle_line(const std::string& line) {
  if (line.rfind("START ", 0) == 0) return plat_start_child(line.substr(6));
  if (line == "STOP") return plat_stop_child();
  if (line == "STATUS") return plat_status_child();
#if defined(_WIN32)
  if (line.rfind("TERM ", 0) == 0) return plat_term_pid(line.substr(5));
#else
  if (line == "HUP") return plat_hup_managed();
  if (line.rfind("HUP ", 0) == 0) return plat_signal_pid(line.substr(4), SIGHUP);
  if (line.rfind("TERM ", 0) == 0) return plat_signal_pid(line.substr(5), SIGTERM);
#endif
  return "ERR unknown\n";
}

void shutdown_child() { (void)plat_stop_child(); }

}  // namespace tunnel_helper
