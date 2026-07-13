#pragma once

#include <string>
#include <vector>

// Real tar archives for multi-file / folder sends (not used for type=file).
std::string create_tar_for_file(const std::string& file_path,
                                const std::string& source_root,
                                std::string* error_out = nullptr);

std::string create_tar_for_directory(const std::string& directory_path,
                                     std::string* error_out = nullptr);

/** Pack one or more files and/or directories into a single tar archive. */
std::string create_tar_for_paths(const std::vector<std::string>& paths,
                                 std::string* error_out = nullptr);

bool extract_tar_to_dir(const std::string& tar_path,
                        const std::string& target_dir,
                        std::string* error_out = nullptr);
