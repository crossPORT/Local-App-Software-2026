#include "rocketbox/sdk.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void run_test() {
    std::cout << "[Test] Attaching Session 1 (Port 1)..." << std::endl;
    auto session1 = rocketbox::Attach(rocketbox::TransportType::Sim, 1);
    assert(!session1->GetSystemId().empty());
    std::cout << "[Test] Session 1 System ID: " << session1->GetSystemId() << std::endl;

    std::cout << "[Test] Attaching Session 2 (Port 2)..." << std::endl;
    auto session2 = rocketbox::Attach(rocketbox::TransportType::Sim, 2);
    assert(!session2->GetSystemId().empty());
    std::cout << "[Test] Session 2 System ID: " << session2->GetSystemId() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[Test] Listing systems from Session 1..." << std::endl;
    auto systems1 = session1->ListSystems();
    assert(systems1.size() >= 1);
    std::cout << "[Test] Systems found: " << systems1.size() << std::endl;
    bool found_peer = false;
    for (const auto& s : systems1) {
        std::cout << "  - " << s.id << " (" << s.name << "): " << s.status << std::endl;
        if (s.id == session2->GetSystemId()) {
            found_peer = true;
            assert(s.status == "reachable");
        }
    }
    assert(found_peer);

    // Set up incoming circuit handler on Session 2
    std::atomic<bool> incoming_triggered{false};
    std::shared_ptr<rocketbox::Connection> received_conn;
    session2->OnIncomingCircuit([&](std::shared_ptr<rocketbox::Connection> conn) {
        std::cout << "[Test] Session 2 received incoming connection from: " << conn->GetPeerSystemId() << std::endl;
        received_conn = conn;
        incoming_triggered = true;
    });

    std::cout << "[Test] Session 1 connecting to Session 2..." << std::endl;
    auto conn1 = session1->Connect(session2->GetSystemId());
    assert(conn1 != nullptr);
    std::cout << "[Test] Connection 1 state: " << conn1->GetState() << std::endl;
    assert(conn1->GetPeerSystemId() == session2->GetSystemId());

    // Wait for Session 2 to receive the connection
    for (int i = 0; i < 20; i++) {
        if (incoming_triggered) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(incoming_triggered);
    assert(received_conn != nullptr);

    // Set up data received handler on Connection 2
    std::atomic<bool> data_received{false};
    std::vector<uint8_t> received_bytes;
    received_conn->OnReceived([&](const std::vector<uint8_t>& bytes) {
        std::cout << "[Test] Session 2 connection received bytes, size: " << bytes.size() << std::endl;
        received_bytes = bytes;
        data_received = true;
    });

    std::cout << "[Test] Session 1 sending payload data..." << std::endl;
    std::vector<uint8_t> test_payload = {'H', 'e', 'l', 'l', 'o', ' ', 'S', 'D', 'K', '!'};
    conn1->Send(test_payload);

    // Wait for Session 2 to receive the payload
    for (int i = 0; i < 20; i++) {
        if (data_received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(data_received);
    assert(received_bytes == test_payload);
    std::cout << "[Test] Payload verified successfully!" << std::endl;

    // Set up closed handler on Session 2
    std::atomic<bool> connection_closed{false};
    received_conn->OnClosed([&](const std::string& reason) {
        std::cout << "[Test] Session 2 connection closed, reason: " << reason << std::endl;
        connection_closed = true;
    });

    std::cout << "[Test] Session 1 closing connection..." << std::endl;
    conn1->Close();

    // Wait for close notification to propagate
    for (int i = 0; i < 20; i++) {
        if (connection_closed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(connection_closed);
    assert(conn1->GetState() == "closed");
    assert(received_conn->GetState() == "closed");

    std::cout << "[Test] All SDK features successfully verified!" << std::endl;
}

int main() {
    std::cout << "[Test] Starting Simulation Daemon..." << std::endl;
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return 1;
    }

    if (pid == 0) {
        // Child process: start node simulation-daemon
        char* args[] = {const_cast<char*>("node"), const_cast<char*>("/home/geoff-whittington/Projects/data-transfer-demo/simulated-hardware/daemon.js"), nullptr};
        execvp("node", args);
        // If execvp fails
        std::cerr << "Failed to start node daemon" << std::endl;
        exit(1);
    }

    // Parent process: Wait a moment for daemon to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    try {
        run_test();
    } catch (const std::exception& e) {
        std::cerr << "[Test Error] SDK Test failed with exception: " << e.what() << std::endl;
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        return 1;
    } catch (...) {
        std::cerr << "[Test Error] SDK Test failed with unknown exception" << std::endl;
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        return 1;
    }

    std::cout << "[Test] Stopping Simulation Daemon..." << std::endl;
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, 0);
    std::cout << "[Test] SDK integration tests passed perfectly!" << std::endl;
    return 0;
}
