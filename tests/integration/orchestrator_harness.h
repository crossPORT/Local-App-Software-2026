#pragma once

#include "fabric_sim.h"
#include "identity_profile.h"
#include "transfer_orchestrator.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

// Helpers for in-process TransferOrchestrator tests over fabric_sim (two ports).

namespace integration {

class SimFabricScope {
public:
    SimFabricScope() {
        fabric_sim_set_enabled(true);
        fabric_sim_reset();
    }
    ~SimFabricScope() {
        fabric_sim_set_enabled(false);
        fabric_sim_reset();
    }
};

inline std::string make_temp_dir(const char* prefix) {
    std::string path = std::string("/tmp/") + prefix + "XXXXXX";
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    if (mkdtemp(buf.data()) == nullptr) {
        return {};
    }
    return std::string(buf.data());
}

inline IdentityProfile make_identity(const std::string& display_name,
                                     const std::string& receive_folder,
                                     ReceiveStatus receive_status) {
    IdentityProfile profile{};
    profile.display_name = display_name;
    profile.team = "Test";
    profile.receive_status = receive_status;
    profile.receive_folder = receive_folder;
    return profile;
}

inline std::string write_temp_file(const std::string& contents) {
    char path_template[] = "/tmp/slsfabric-int-XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        return {};
    }
    const ssize_t written = ::write(fd, contents.data(), contents.size());
    ::close(fd);
    if (written < 0 || static_cast<std::size_t>(written) != contents.size()) {
        std::remove(path_template);
        return {};
    }
    return path_template;
}

class TrackedOrchestrator {
public:
    TrackedOrchestrator(int port_index, IdentityProfile identity)
        : orchestrator_(std::make_unique<TransferOrchestrator>(
              port_index,
              std::move(identity),
              [this](const OrchestratorUiState& state) {
                  std::lock_guard<std::mutex> lock(mutex_);
                  state_ = state;
              })) {}

    void start() {
        orchestrator_->start(/*run_wiring_probe=*/false, /*enable_listener=*/true);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    void stop() {
        orchestrator_->stop();
    }

    TransferOrchestrator& orchestrator() {
        return *orchestrator_;
    }

    OrchestratorUiState snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    bool wait_until(const std::function<bool(const OrchestratorUiState&)>& predicate,
                    std::chrono::milliseconds timeout) const {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate(snapshot())) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        return predicate(snapshot());
    }

    bool wait_for_peer_online(const std::string& display_name,
                              std::chrono::milliseconds timeout) const {
        return wait_until(
            [&](const OrchestratorUiState& state) {
                for (const PeerEntry& peer : state.roster) {
                    if (peer.display_name == display_name && peer.online) {
                        return true;
                    }
                }
                return false;
            },
            timeout);
    }

    bool wait_for_notification_substring(const std::string& needle,
                                       std::chrono::milliseconds timeout) const {
        return wait_until(
            [&](const OrchestratorUiState& state) {
                return !state.busy && state.notification.find(needle) != std::string::npos;
            },
            timeout);
    }

private:
    mutable std::mutex mutex_;
    OrchestratorUiState state_{};
    std::unique_ptr<TransferOrchestrator> orchestrator_;
};

}  // namespace integration
