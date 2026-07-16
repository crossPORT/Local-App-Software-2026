#pragma once

#include <string>

namespace tunnel_helper {

std::string plat_start_child(const std::string& cmdline);
std::string plat_stop_child();
std::string plat_status_child();
#if defined(_WIN32)
std::string plat_term_pid(const std::string& arg);
#else
std::string plat_hup_managed();
std::string plat_signal_pid(const std::string& arg, int sig);
#endif

}  // namespace tunnel_helper
