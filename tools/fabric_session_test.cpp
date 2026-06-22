#include "identity_profile.h"
#include "transfer_orchestrator.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

std::string peer_name_for_port(const IdentityProfile& identity, int port) {
    for (const PeerConfig& peer : identity.peers) {
        if (peer.port_index == port) {
            return peer.display_name;
        }
    }
    return {};
}

int run_receiver(int recv_port,
                 const std::string& config_path,
                 const std::string& done_file) {
    IdentityProfile identity{};
    load_identity_profile(recv_port, config_path, identity);
    identity.receive_status = ReceiveStatus::Open;

    bool recv_done = false;
    bool recv_ok = false;

    TransferOrchestrator receiver(
        recv_port,
        identity,
        [&](const OrchestratorUiState& state) {
            if (!state.notification.empty()) {
                recv_done = true;
                recv_ok = state.error_message.empty();
            }
        });

    receiver.start(false, true);

    {
        std::ofstream ready(done_file + ".ready", std::ios::trunc);
        ready << "ready";
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(75);
    while (std::chrono::steady_clock::now() < deadline) {
        if (recv_done) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    receiver.stop();

    std::ofstream out(done_file, std::ios::trunc);
    out << (recv_done && recv_ok ? "ok" : "fail");
    std::cout << "receiver: recv_done=" << recv_done << " recv_ok=" << recv_ok << '\n';
    return (recv_done && recv_ok) ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    const int recv_port = (argc >= 2) ? std::atoi(argv[1]) : 0;
    const int send_port = (argc >= 3) ? std::atoi(argv[2]) : 1;
    const std::string config_path = (argc >= 4) ? argv[3] : "demo-config/shared.conf";
    const std::string done_file = "/tmp/fabric-session-recv-done";

    std::remove(done_file.c_str());
    std::remove((done_file + ".ready").c_str());

    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    if (child == 0) {
        return run_receiver(recv_port, config_path, done_file);
    }

    IdentityProfile send_identity{};
    load_identity_profile(send_port, config_path, send_identity);
    send_identity.receive_status = ReceiveStatus::Open;

    bool send_done = false;
    bool send_ok = false;

    TransferOrchestrator sender(
        send_port,
        send_identity,
        [&](const OrchestratorUiState& state) {
            if (!state.busy && state.status_message.find("complete") != std::string::npos) {
                send_done = true;
                send_ok = state.error_message.empty();
            }
            if (!state.error_message.empty() && !state.busy) {
                std::cerr << "sender error: " << state.error_message << '\n';
            }
        });

    sender.start(false, false);

    const std::string ready_file = done_file + ".ready";
    const auto ready_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < ready_deadline) {
        std::ifstream ready(ready_file);
        if (ready.good()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::remove(ready_file.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string target = peer_name_for_port(send_identity, recv_port);
    if (target.empty()) {
        IdentityProfile recv_identity{};
        load_identity_profile(recv_port, config_path, recv_identity);
        target = recv_identity.display_name;
    }

    std::cout << "sender targeting peer: " << target << '\n';

    if (!sender.send_to_peer(target, {"/tmp/small-file.bin"}, "booth test")) {
        std::cerr << "send_to_peer failed to start\n";
        kill(child, SIGTERM);
        waitpid(child, nullptr, 0);
        return 1;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(75);
    while (std::chrono::steady_clock::now() < deadline) {
        if (send_done) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sender.stop();

    int child_status = 0;
    waitpid(child, &child_status, 0);

    std::string recv_result;
    {
        std::ifstream in(done_file);
        std::getline(in, recv_result);
    }
    std::remove(done_file.c_str());

    const bool recv_ok = recv_result == "ok";
    std::cout << "sender: send_done=" << send_done << " send_ok=" << send_ok
              << " recv_result=" << recv_result << '\n';

    return (send_done && send_ok && recv_ok) ? 0 : 1;
}
