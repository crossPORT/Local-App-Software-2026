#include "usb_transfer.h"

#include <iostream>
#include <libusb.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file-to-loopback>\n";
        return 1;
    }

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        std::cerr << "libusb_init failed\n";
        return 1;
    }

    const int devices = count_fabric_devices(ctx);
    std::cout << "fabric devices: " << devices << '\n';
    if (devices < 2) {
        std::cerr << "Need 2 cables from this PC to two CON ports\n";
        libusb_exit(ctx);
        return 2;
    }

    auto result = loopback_transfer_core(ctx, argv[1], 0, 1,
        [](uint64_t done, uint64_t total, double elapsed) {
            if (total > 0) {
                const int pct = static_cast<int>((done * 100) / total);
                std::cout << "\rsend " << pct << "% (" << elapsed << "s)" << std::flush;
            }
        });
    std::cout << '\n';
    std::cout << "ok=" << result.ok
              << " bytes=" << result.bytes_transferred
              << " mbps=" << result.mbps
              << " err=" << result.error_message << '\n';

    libusb_exit(ctx);
    return result.ok ? 0 : 1;
}
