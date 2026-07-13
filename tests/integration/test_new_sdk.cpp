#include "rocketbox/sdk.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void run_test() {
    std::cout << "[Test] Connecting transport 1 (sim port 1)..." << std::endl;
    auto t1 = rocketbox::create_rocketbox_transport(rocketbox::TransportMode::Sim, 1);
    t1->connect();
    assert(!t1->system_id().empty());
    std::cout << "[Test] System 1: " << t1->system_id() << std::endl;

    std::cout << "[Test] Connecting transport 2 (sim port 2)..." << std::endl;
    auto t2 = rocketbox::create_rocketbox_transport(rocketbox::TransportMode::Sim, 2);
    t2->connect();
    assert(!t2->system_id().empty());
    std::cout << "[Test] System 2: " << t2->system_id() << std::endl;

    std::atomic<bool> got{false};
    std::vector<uint8_t> received;
    t2->on_data_message([&](const std::vector<uint8_t>& body) {
        // Body is ROCKETBX payload (may be raw or u32-framed).
        received = body;
        got = true;
    });

    t1->ensure_circuit(t2->system_id());
    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', ' ', 'S', 'D', 'K', '!'};
    t1->send_bytes(payload);

    for (int i = 0; i < 50 && !got; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(got);
    assert(received == payload);
    std::cout << "[Test] Payload verified!" << std::endl;

    t1->clear_circuit();
    t1->disconnect();
    t2->disconnect();
    std::cout << "[Test] SDK transport verified." << std::endl;
}

int main() {
    std::cout << "[Test] Starting simulated-hardware..." << std::endl;
    pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid == 0) {
        char* args[] = {const_cast<char*>("node"),
                        const_cast<char*>("simulated-hardware/daemon.js"), nullptr};
        chdir("/home/geoff-whittington/Projects/data-transfer-demo");
        execvp("node", args);
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    int rc = 0;
    try {
        run_test();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        rc = 1;
    }
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, 0);
    return rc;
}
