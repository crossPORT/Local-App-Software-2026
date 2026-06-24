#pragma once

#include <string>
#include <vector>

namespace platform {

// User home directory (HOME on Unix, USERPROFILE on Windows).
std::string home_directory();

// Expands a leading ~/ prefix using home_directory().
std::string expand_home(std::string path);

std::string temp_directory();

std::string default_booth_log_path();

// Per-user identity settings file when --config is not passed.
std::string default_identity_config_path();

// Integration-test helper path shared by booth_cli / fabric_session_test.
std::string shared_booth_path_config();

// Creates a new empty file in the temp directory; returns its path or {}.
std::string create_empty_temp_file(const std::string& prefix, const std::string& suffix = "");

// Writes text to a new temp file; returns its path or {}.
std::string write_temp_text_file(const std::string& prefix, const std::string& content);

bool run_command(const std::vector<std::string>& args, std::string* error_out = nullptr);

bool stderr_is_tty();

std::string format_timestamp_iso8601_ms();

}  // namespace platform
