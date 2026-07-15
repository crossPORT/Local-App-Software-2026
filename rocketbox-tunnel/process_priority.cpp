#include "process_priority.hpp"

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <sys/resource.h>
#endif

bool rocketbox_raise_process_priority(std::string* detail) {
#if defined(_WIN32)
  if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
    if (detail) {
      *detail = "SetPriorityClass HIGH failed err=" + std::to_string(GetLastError());
    }
    return false;
  }
  (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  if (detail) *detail = "priority=HIGH";
  return true;
#else
  // Prefer -10; fall back if CAP_SYS_NICE / privileges are missing.
  for (int nice : {-10, -5}) {
    errno = 0;
    if (setpriority(PRIO_PROCESS, 0, nice) == 0) {
      if (detail) *detail = "nice=" + std::to_string(nice);
      return true;
    }
  }
  if (detail) {
    *detail = std::string("setpriority failed: ") + std::strerror(errno);
  }
  return false;
#endif
}
