#include "usb_transfer.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <libusb.h>
#include <thread>
#include <vector>

namespace {

bool files_equal(const std::string& a, const std::string& b) {
    std::ifstream left(a, std::ios::binary);
    std::ifstream right(b, std::ios::binary);
    if (!left || !right) {
        return false;
    }

    left.seekg(0, std::ios::end);
    right.seekg(0, std::ios::end);
    if (left.tellg() != right.tellg()) {
        return false;
    }
    left.seekg(0, std::ios::beg);
    right.seekg(0, std::ios::beg);

    std::vector<char> la(64 * 1024);
    std::vector<char> ra(64 * 1024);
    while (left && right) {
        left.read(la.data(), static_cast<std::streamsize>(la.size()));
        right.read(ra.data(), static_cast<std::streamsize>(ra.size()));
        if (left.gcount() != right.gcount()) {
            return false;
        }
        if (left.gcount() > 0
            && std::memcmp(la.data(), ra.data(), static_cast<std::size_t>(left.gcount())) != 0) {
            return false;
        }
    }
    return left.eof() && right.eof();
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <file-to-send> [recv-port send-port]\n"
                  << "  Mimics two GUI instances: receive on port 0, send on port 1.\n";
        return 1;
    }

    const int recv_port = (argc >= 3) ? std::atoi(argv[2]) : 0;
    const int send_port = (argc >= 4) ? std::atoi(argv[3]) : 1;

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        std::cerr << "libusb_init failed\n";
        return 1;
    }

    const int devices = count_fabric_devices(ctx);
    std::cout << "fabric devices: " << devices << '\n';
    if (devices < 2) {
        std::cerr << "Need 2 cables (ports " << recv_port << " and " << send_port << ")\n";
        libusb_exit(ctx);
        return 2;
    }

    const std::string src = argv[1];
    const std::string dst = src + ".pair_recv";

    TransferResult recv_result{};
    TransferResult send_result{};

    std::cout << "Starting receive on port " << recv_port << " (like GUI --port "
              << recv_port << ")...\n";

    std::thread recv_thread([&]() {
        recv_result = receive_file_core(ctx, dst, recv_port,
            [](uint64_t done, uint64_t total, double elapsed) {
                if (total > 0) {
                    const int pct = static_cast<int>((done * 100) / total);
                    std::cout << "\rrecv port " << pct << "% (" << elapsed << "s)"
                              << std::flush;
                }
            });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "\nSending on port " << send_port << " (like GUI --port "
              << send_port << ")...\n";

    send_result = send_file_core(ctx, src, send_port,
        [](uint64_t done, uint64_t total, double elapsed) {
            if (total > 0) {
                const int pct = static_cast<int>((done * 100) / total);
                std::cout << "\rsend port " << pct << "% (" << elapsed << "s)"
                          << std::flush;
            }
        });

    recv_thread.join();
    std::cout << '\n';

    std::cout << "send  ok=" << send_result.ok
              << " bytes=" << send_result.bytes_transferred
              << " mbps=" << send_result.mbps
              << " err=" << send_result.error_message << '\n';
    std::cout << "recv  ok=" << recv_result.ok
              << " bytes=" << recv_result.bytes_transferred
              << " mbps=" << recv_result.mbps
              << " err=" << recv_result.error_message << '\n';

    bool data_ok = false;
    if (send_result.ok && recv_result.ok) {
        data_ok = files_equal(src, dst);
        std::cout << "data match: " << (data_ok ? "PASS" : "FAIL") << '\n';
    }

    std::remove(dst.c_str());
    libusb_exit(ctx);

    return (send_result.ok && recv_result.ok && data_ok) ? 0 : 1;
}
