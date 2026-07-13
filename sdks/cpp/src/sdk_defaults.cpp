#include "rocketbox/sdk.h"

namespace rocketbox {

FileTransferResult RocketBoxTransport::switch_port(int) {
    return TransferResult{false, 0, 0, 0.0, 0.0, "switch_port unsupported"};
}

FileTransferResult RocketBoxTransport::switch_port_if_needed(int dest) {
    return switch_port(dest);
}

FileTransferResult RocketBoxTransport::send_file_on_port(int, const std::string&, FileProgressFn,
                                                        unsigned, uint8_t) {
    return TransferResult{false, 0, 0, 0.0, 0.0, "send_file_on_port unsupported"};
}

FileTransferResult RocketBoxTransport::receive_file_on_port(int, const std::string&, FileProgressFn,
                                                           unsigned, uint8_t) {
    return TransferResult{false, 0, 0, 0.0, 0.0, "receive_file_on_port unsupported"};
}

FileTransferResult RocketBoxTransport::loopback_files(const std::string&, int, int, FileProgressFn) {
    return TransferResult{false, 0, 0, 0.0, 0.0, "loopback_files unsupported"};
}

}  // namespace rocketbox
