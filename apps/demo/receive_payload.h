#pragma once

#include "usb_transfer.h"

#include <cstdint>
#include <string>

// Result of removing a partial/aborted inbound payload file.
struct PartialReceiveCleanup {
    bool existed_before = false;
    uint64_t bytes_removed = 0;
    bool removed = false;
    // True when nothing remains at path (including when it never existed).
    bool path_clear = false;
};

// Destination path for an inbound payload under receive_folder.
std::string build_inbound_payload_path(const std::string& receive_folder,
                                       const std::string& payload_name);

// True when a failed USB receive should trigger partial-file cleanup.
bool should_remove_partial_after_receive(const TransferResult& usb_result);

// Unlink a failed/aborted receive payload so the inbox never keeps a corrupt
// file (cable pulled, timeout, sender killed mid-stream, tar extract fail, …).
PartialReceiveCleanup cleanup_partial_receive_file(const std::string& out_path);

// Policy + cleanup for a completed USB receive attempt. On failure, removes
// any partial file at out_path and returns the USB error for the UI layer.
struct FailedInboundReceive {
    PartialReceiveCleanup cleanup;
    std::string error_message;
};

FailedInboundReceive handle_failed_inbound_receive(const TransferResult& usb_result,
                                                  const std::string& out_path);
