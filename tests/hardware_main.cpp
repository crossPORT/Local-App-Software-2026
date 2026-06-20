#include "test_util.h"
#include "hardware/hw_fixture.h"

#include "usb_transfer.h"

#include <iostream>

// CTest treats this exit code as "skipped" (SKIP_RETURN_CODE in CMakeLists).
constexpr int kSkipExitCode = 77;

libusb_context* g_hw_ctx = nullptr;

int main(int argc, char** argv) {
    if (libusb_init(&g_hw_ctx) != 0) {
        std::cerr << "SKIP: libusb_init failed\n";
        return kSkipExitCode;
    }

    const int devices = count_fabric_devices(g_hw_ctx);
    if (devices < 2) {
        std::cerr << "SKIP: hardware tests need 2 fabric devices (found "
                  << devices << "). Plug in both USB cables.\n";
        libusb_exit(g_hw_ctx);
        return kSkipExitCode;
    }

    const int rc = fabric_test::run_all(argc, argv);
    libusb_exit(g_hw_ctx);
    return rc;
}
