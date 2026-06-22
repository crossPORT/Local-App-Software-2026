#pragma once

#include <string>

struct FabricSendMeta {
    std::string type = "file";  // "file" today; "tar" reserved for real archive payloads
    std::string relative_name;
};

bool fabric_meta_is_file(const FabricSendMeta& meta);
// True when payload is a real ustar archive (not implemented on send yet).
bool fabric_meta_is_tar_archive(const FabricSendMeta& meta);

// Writes a tiny temp file the GUI sends via ordinary send_file_core.
std::string write_meta_temp_file(const FabricSendMeta& meta);

// Parses a file received via ordinary receive_file_core.
bool read_meta_file(const std::string& path, FabricSendMeta& out);

// Safe join: target_dir + relative_name (strips path traversal).
std::string target_path_for_name(const std::string& target_dir,
                                 const std::string& relative_name);

// Create parent directories for a target file path.
bool ensure_parent_directories(const std::string& file_path,
                               std::string* error_out = nullptr);
