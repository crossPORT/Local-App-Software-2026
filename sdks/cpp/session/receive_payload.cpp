#include "receive_payload.h"

#include <cstdio>
#include <filesystem>

std::string build_inbound_payload_path(const std::string& receive_folder,
                                       const std::string& payload_name) {
    std::string out_path = receive_folder;
    if (!out_path.empty() && out_path.back() != '/') {
        out_path += '/';
    }
    out_path += payload_name.empty() ? "incoming.bin" : payload_name;
    return out_path;
}

bool should_remove_partial_after_receive(const TransferResult& usb_result) {
    return !usb_result.ok;
}

PartialReceiveCleanup cleanup_partial_receive_file(const std::string& out_path) {
    PartialReceiveCleanup result;
    if (out_path.empty()) {
        result.path_clear = true;
        return result;
    }

    std::error_code ec;
    result.existed_before = std::filesystem::exists(out_path, ec);
    if (!result.existed_before) {
        result.path_clear = true;
        return result;
    }

    if (std::filesystem::is_regular_file(out_path, ec)) {
        result.bytes_removed = static_cast<uint64_t>(std::filesystem::file_size(out_path, ec));
    }

    result.removed = (std::remove(out_path.c_str()) == 0);
    result.path_clear = !std::filesystem::exists(out_path, ec);
    return result;
}

FailedInboundReceive handle_failed_inbound_receive(const TransferResult& usb_result,
                                                   const std::string& out_path) {
    FailedInboundReceive out;
    out.error_message = usb_result.error_message;
    if (should_remove_partial_after_receive(usb_result)) {
        out.cleanup = cleanup_partial_receive_file(out_path);
    } else {
        out.cleanup.path_clear = true;
    }
    return out;
}
