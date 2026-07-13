#pragma once

#include "usb_transfer.h"

// Re-export core transfer types for RocketBoxTransport native file ops.
namespace rocketbox {
using FileTransferResult = TransferResult;
using FileProgressFn = ProgressCallback;
}  // namespace rocketbox
