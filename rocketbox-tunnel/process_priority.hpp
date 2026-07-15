#pragma once

#include <string>

/** Best-effort raise of process scheduling priority. Returns false if unchanged. */
bool rocketbox_raise_process_priority(std::string* detail);
