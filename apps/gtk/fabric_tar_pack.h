#pragma once

#include <string>

// Real tar archives for future multi-file / folder sends (not used for type=file).
std::string create_tar_for_file(const std::string& file_path,
                                const std::string& source_root,
                                std::string* error_out = nullptr);

bool extract_tar_to_dir(const std::string& tar_path,
                        const std::string& target_dir,
                        std::string* error_out = nullptr);
