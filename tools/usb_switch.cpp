#include "usb_transfer.h"
#include "usb_protocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libusb-1.0/libusb.h>
#include <string>

namespace {

void usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s --port N --dest D\n"
                 "  --port N  libusb sort index (0-based)\n"
                 "  --dest D  port 1-4 to link, or 0 to clear\n",
                 argv0);
}

bool parse_int_arg(const char* s, int* out) {
    if (!s || !*s) {
        return false;
    }
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    *out = static_cast<int>(v);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    int port_index = -1;
    int dest_port = -1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            if (!parse_int_arg(argv[++i], &port_index)) {
                usage(argv[0]);
                return 2;
            }
        } else if (std::strcmp(argv[i], "--dest") == 0 && i + 1 < argc) {
            if (!parse_int_arg(argv[++i], &dest_port)) {
                usage(argv[0]);
                return 2;
            }
        } else if (std::strcmp(argv[i], "-h") == 0
                   || std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (port_index < 0 || dest_port < 0 || dest_port > 15) {
        usage(argv[0]);
        return 2;
    }

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        std::fprintf(stderr, "libusb_init failed\n");
        return 1;
    }

    const TransferResult result = switch_port_core(ctx, port_index, dest_port);
    libusb_exit(ctx);

    if (!result.ok) {
        std::fprintf(stderr, "switch failed: %s\n", result.error_message.c_str());
        return 1;
    }

    std::printf("ok port=%d dest=%d bytes=%llu (%.3fs)\n",
                port_index,
                dest_port,
                static_cast<unsigned long long>(result.bytes_transferred),
                result.seconds);
    return 0;
}
