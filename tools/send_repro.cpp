// Headless repro for drop->send path (no wx drag-and-drop UI).
#include "fabric_meta_file.h"
#include "transfer_controller.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    const int port = (argc >= 2) ? std::atoi(argv[1]) : 1;
    const char* path = (argc >= 3) ? argv[2] : "/tmp/sls-drop-test.dat";

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "sls fabric drop repro\n";
    }

    std::atomic<bool> done{false};
    TransferController ctrl(port, [&](const TransferUiState& state) {
        std::cout << "busy=" << state.busy
                  << " status=" << state.status_message
                  << " err=" << state.error_message << '\n';
        if (!state.busy) {
            done.store(true);
        }
    });

    FabricSendMeta meta{};
    meta.type = "file";
    meta.relative_name = "sls-drop-test.dat";
    std::cout << "send_transfer port=" << port << " path=" << path << '\n';
    ctrl.send_transfer(path, meta, "");

    for (int i = 0; i < 300 && !done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return done.load() ? 0 : 2;
}
