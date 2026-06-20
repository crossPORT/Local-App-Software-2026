#include "transfer_orchestrator.h"

#include "booth_log.h"
#include "fabric_link_policy.h"
#include "fabric_meta_file.h"
#include "fabric_tar_pack.h"
#include "receive_payload.h"
#include "session_listener.h"
#include "usb_protocol.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace {

int64_t steady_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

uint64_t file_size_bytes(const std::string& path) {
    std::error_code ec;
    return static_cast<uint64_t>(std::filesystem::file_size(path, ec));
}

uint32_t count_files_in_paths(const std::vector<std::string>& paths) {
    uint32_t count = 0;
    for (const std::string& path : paths) {
        if (std::filesystem::is_directory(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    ++count;
                }
            }
        } else if (std::filesystem::is_regular_file(path)) {
            ++count;
        }
    }
    return count;
}

std::string basename_of(const std::string& path) {
    const std::filesystem::path p(path);
    return p.filename().string();
}

}  // namespace

namespace {

void load_shared_booth_path(int& send_port, int& recv_port) {
    std::ifstream file("/tmp/slsfabric-booth-path.conf");
    if (!file.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        try {
            if (key == "send_port") {
                send_port = std::stoi(value);
            } else if (key == "recv_port") {
                recv_port = std::stoi(value);
            }
        } catch (...) {
        }
    }
}

}  // namespace

TransferOrchestrator::TransferOrchestrator(int port_index,
                                           IdentityProfile identity,
                                           UiCallback on_ui_update)
    : port_index_(port_index)
    , identity_(std::move(identity))
    , on_ui_update_(std::move(on_ui_update)) {
    state_.identity = identity_;
    if (identity_.transfer_timeout_ms > 0) {
        set_payload_timeout_ms(static_cast<unsigned>(identity_.transfer_timeout_ms));
    }
    if (identity_.usb_inflight_mb > 0) {
        set_inflight_budget_mb(static_cast<unsigned>(identity_.usb_inflight_mb));
    }
    booth_log(port_index_, "usb_inflight",
              "usbfs_limit_mb=" + std::to_string(usbfs_limit_mb())
                  + " queue_depth=" + std::to_string(inflight_queue_depth()));
    roster_.seed_from_config(identity_.peers);
    state_.roster = roster_.peers();

    controller_ = std::make_unique<TransferController>(port_index_, nullptr);

    listener_ = std::make_unique<SessionListener>(
        controller_.get(),
        port_index_,
        [this](const FabricSessionMessage& message) { on_session_message(message); });
    publish_state();
}

TransferOrchestrator::~TransferOrchestrator() {
    stop();
}

void TransferOrchestrator::start(bool run_wiring_probe, bool enable_listener) {
    publish_state();
    if (startup_thread_.joinable()) {
        startup_thread_.join();
    }
    shutting_down_.store(false);
    startup_thread_ = std::thread([this, run_wiring_probe, enable_listener]() {
        booth_log(port_index_, "startup", "wiring_probe=" + std::string(run_wiring_probe ? "yes" : "no"));
        // Only port 0 runs loopback wiring probe (opens both cables briefly).
        // Port 1 waits so the two GUI processes do not fight for USB handles.
        if (run_wiring_probe && port_index_ > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        if (shutting_down_.load()) {
            return;
        }
        if (run_wiring_probe && port_index_ == 0) {
            ensure_wiring();
        } else if (run_wiring_probe && port_index_ > 0) {
            load_shared_booth_path(working_send_port_, working_recv_port_);
        }
        if (shutting_down_.load()) {
            return;
        }
        if (enable_listener) {
            ensure_listener_started();
        }
        publish_state();
        start_presence_loop();
    });
}

void TransferOrchestrator::start_presence_loop() {
    if (presence_thread_.joinable()) {
        return;
    }
    presence_thread_ = std::thread([this]() {
        while (!shutting_down_.load(std::memory_order_acquire)) {
            tick_presence();
            for (int i = 0; i < 10 && !shutting_down_.load(std::memory_order_acquire); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
}

void TransferOrchestrator::ensure_listener_started() {
    if (listener_started_) {
        return;
    }
    listener_->start();
    listener_started_ = true;
    booth_log(port_index_,
              "listener_started",
              "working_send=" + std::to_string(working_send_port_)
                  + " working_recv=" + std::to_string(working_recv_port_));
}

void TransferOrchestrator::stop() {
    shutting_down_.store(true, std::memory_order_release);
    if (controller_) {
        controller_->request_shutdown();
    }
    if (listener_) {
        listener_->stop();
    }
    if (presence_thread_.joinable()) {
        presence_thread_.join();
    }
    if (startup_thread_.joinable()) {
        startup_thread_.join();
    }
    if (payload_thread_.joinable()) {
        std::atomic<bool> joined{false};
        std::thread reaper([this, &joined] {
            payload_thread_.join();
            joined.store(true, std::memory_order_release);
        });
        for (int i = 0; i < 30 && !joined.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (joined.load(std::memory_order_acquire)) {
            if (reaper.joinable()) {
                reaper.join();
            }
        } else {
            booth_log(port_index_, "shutdown", "payload thread detach on timeout");
            if (reaper.joinable()) {
                reaper.detach();
            }
            payload_thread_.detach();
        }
    }
}

bool TransferOrchestrator::is_busy() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_.busy;
}

IdentityProfile TransferOrchestrator::identity() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return identity_;
}

OrchestratorUiState TransferOrchestrator::snapshot() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

void TransferOrchestrator::set_identity(IdentityProfile identity) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        identity_ = std::move(identity);
        state_.identity = identity_;
        if (identity_.transfer_timeout_ms > 0) {
            set_payload_timeout_ms(static_cast<unsigned>(identity_.transfer_timeout_ms));
        }
        if (identity_.usb_inflight_mb > 0) {
            set_inflight_budget_mb(static_cast<unsigned>(identity_.usb_inflight_mb));
        }
        roster_.seed_from_config(identity_.peers);
        state_.roster = roster_.peers();
    }
    save_identity_profile(identity_);
    publish_state();
}

void TransferOrchestrator::publish_state() {
    if (shutting_down_.load() || !on_ui_update_) {
        return;
    }
    on_ui_update_(state_);
}

void TransferOrchestrator::ensure_wiring() {
    if (wiring_checked_) {
        return;
    }
    wiring_checked_ = true;

    // The fabric routes both directions natively, so there is no working-port
    // discovery to do. The probe is disabled.
    booth_log(port_index_, "wiring_probe", "skipped (native bidirectional routing)");

    publish_state();
}

bool TransferOrchestrator::stage_paths(const std::vector<std::string>& paths,
                                         const std::string& note,
                                         StagedPayload& out,
                                         std::string* error_out) {
    if (paths.empty()) {
        if (error_out) {
            *error_out = "No files selected";
        }
        return false;
    }

    out = StagedPayload{};
    out.file_count = count_files_in_paths(paths);

    if (paths.size() == 1 && std::filesystem::is_regular_file(paths[0])) {
        out.path = paths[0];
        out.display_name = basename_of(paths[0]);
        out.payload_type = "file";
        out.total_bytes = file_size_bytes(paths[0]);
        return true;
    }

    if (paths.size() == 1 && std::filesystem::is_directory(paths[0])) {
        std::string tar_error;
        out.path = create_tar_for_directory(paths[0], &tar_error);
        if (out.path.empty()) {
            if (error_out) {
                *error_out = tar_error;
            }
            return false;
        }
        out.display_name = basename_of(paths[0]) + ".tar";
        out.payload_type = "tar";
        out.is_temp = true;
        out.total_bytes = file_size_bytes(out.path);
        return true;
    }

    std::string tar_error;
    out.path = create_tar_for_file(paths[0], "", &tar_error);
    if (out.path.empty()) {
        if (error_out) {
            *error_out = tar_error.empty() ? "Could not stage files" : tar_error;
        }
        return false;
    }
    out.display_name = "transfer-bundle.tar";
    out.payload_type = "tar";
    out.is_temp = true;
    out.total_bytes = file_size_bytes(out.path);
    (void)note;
    return true;
}

bool TransferOrchestrator::send_session_with_routing(const FabricSessionMessage& message,
                                                     bool reverse_path,
                                                     std::string* error_out) {
    (void)reverse_path;
    const bool ok = send_session_message(*controller_, port_index_, message, error_out);
    booth_log(port_index_,
              ok ? "session_send_ok" : "session_send_fail",
              session_kind_to_string(message.kind)
                  + (error_out && !error_out->empty() ? " err=" + *error_out : ""));
    return ok;
}

bool TransferOrchestrator::send_to_peer(const std::string& peer_name,
                                          const std::vector<std::string>& paths,
                                          const std::string& note) {
    if (is_busy()) {
        return false;
    }

    const auto peer = roster_.find_by_name(peer_name);
    if (!peer) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.error_message = "Peer not found: " + peer_name;
        publish_state();
        return false;
    }

    StagedPayload staged{};
    std::string stage_error;
    if (!stage_paths(paths, note, staged, &stage_error)) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.error_message = stage_error;
        publish_state();
        return false;
    }

    FabricSessionMessage offer{};
    offer.kind = SessionMessageKind::Offer;
    offer.session_id = make_session_id();
    offer.from_name = identity_.display_name;
    offer.team = identity_.team;
    offer.to_name = peer->display_name;
    offer.note = note;
    offer.payload_type = staged.payload_type;
    offer.payload_name = staged.display_name;
    offer.file_count = staged.file_count;
    offer.total_bytes = staged.total_bytes;

    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.busy = true;
        state_.waiting_for_partner = true;
        state_.status_message = "Sending offer to " + peer->display_name + "…";
        state_.error_message.clear();
    }
    publish_state();

    payload_thread_ = std::thread([this, staged, offer, peer]() {
        listener_->pause();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::string send_error;
        const bool sent = send_session_with_routing(offer, false, &send_error);
        ensure_listener_started();
        listener_->resume();

        if (!sent) {
            if (staged.is_temp) {
                std::remove(staged.path.c_str());
            }
            finish_transfer(false, "Send failed", send_error);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            staged_payload_ = staged;
            outbound_offer_ = offer;
            awaiting_ready_ = false;
            state_.waiting_for_partner = true;
            state_.status_message = "Waiting for " + peer->display_name
                + " to accept… (" + std::to_string(usb_protocol::kAcceptTimeoutSec) + "s)";
            state_.error_message.clear();
        }
        publish_state();

        const auto now0 = std::chrono::steady_clock::now();
        const auto accept_deadline = now0 + std::chrono::seconds(usb_protocol::kAcceptTimeoutSec);
        std::chrono::steady_clock::time_point ready_deadline{};
        bool accepted_seen = false;
        int last_shown_remaining = -1;

        while (true) {
            bool waiting = false;
            bool has_offer = false;
            bool accepted = false;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                waiting = state_.waiting_for_partner;
                has_offer = static_cast<bool>(outbound_offer_);
                accepted = awaiting_ready_;
            }
            // Transfer either started (busy) or was reset elsewhere.
            if (!waiting || !has_offer) {
                return;
            }

            const auto now = std::chrono::steady_clock::now();

            if (!accepted) {
                if (now >= accept_deadline) {
                    if (staged.is_temp) {
                        std::remove(staged.path.c_str());
                    }
                    outbound_offer_.reset();
                    staged_payload_ = {};
                    finish_transfer(false,
                                    "Timed out",
                                    "No response from " + peer->display_name
                                        + " within "
                                        + std::to_string(usb_protocol::kAcceptTimeoutSec) + "s");
                    return;
                }
                const int remaining = 1 + static_cast<int>(
                    std::chrono::duration_cast<std::chrono::seconds>(accept_deadline - now).count());
                if (remaining != last_shown_remaining) {
                    last_shown_remaining = remaining;
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (state_.waiting_for_partner && !awaiting_ready_) {
                        state_.status_message = "Waiting for " + peer->display_name
                            + " to accept… (" + std::to_string(remaining) + "s)";
                        publish_state();
                    }
                }
            } else {
                if (!accepted_seen) {
                    accepted_seen = true;
                    ready_deadline = now + std::chrono::seconds(usb_protocol::kReadyTimeoutSec);
                }
                if (now >= ready_deadline) {
                    if (staged.is_temp) {
                        std::remove(staged.path.c_str());
                    }
                    outbound_offer_.reset();
                    staged_payload_ = {};
                    finish_transfer(false,
                                    "Timed out",
                                    peer->display_name
                                        + " accepted but did not start receiving in time");
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
    return true;
}

void TransferOrchestrator::on_session_message(const FabricSessionMessage& message) {
    booth_log(port_index_,
              "session_recv",
              session_kind_to_string(message.kind) + " from=" + message.from_name
                  + " to=" + message.to_name + " session=" + message.session_id);
    switch (message.kind) {
        case SessionMessageKind::Offer:
            handle_offer(message);
            break;
        case SessionMessageKind::Accept:
            handle_accept(message);
            break;
        case SessionMessageKind::Decline:
            handle_decline(message);
            break;
        case SessionMessageKind::Ready:
            handle_ready(message);
            break;
        case SessionMessageKind::Announce:
            // Legacy peers may still send announces; ignore (presence is enumeration-only).
            break;
        case SessionMessageKind::Unknown:
            break;
    }
}

void TransferOrchestrator::tick_presence() {
    if (shutting_down_.load(std::memory_order_acquire)) {
        return;
    }

    const int64_t now = steady_now_ms();

    // Throttle USB enumeration (~1s); read-only, no interface claim.
    if (last_probe_ms_ == 0 || now - last_probe_ms_ >= 900) {
        cached_devices_ = controller_->fabric_device_count();
        cached_port_ok_ = controller_->fabric_port_available();
        last_probe_ms_ = now;
    }
    const int devices_seen = cached_devices_;
    const bool port_ok = cached_port_ok_;
    const bool fabric_just_lost = !port_ok && fabric_was_connected_;

    if (fabric_just_lost) {
        roster_.set_all_peers_offline();
        bool was_busy = false;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            was_busy = state_.busy;
        }
        if (was_busy) {
            finish_transfer(false,
                            "Fabric disconnected",
                            "USB cable unplugged or power change");
        }
        publish_state();
    }

    const bool fabric_connected = fabric_link::fabric_connected_from_enumeration(port_ok);

    if (fabric_connected) {
        roster_.set_all_peers_online();
    } else {
        roster_.set_all_peers_offline();
    }

    fabric_was_connected_ = port_ok;

    const auto roster = roster_.peers();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.fabric_connected = fabric_connected;
        state_.fabric_devices_seen = devices_seen;
        state_.fabric_port_open = port_ok;
        state_.roster = roster;
    }
    publish_state();
}

void TransferOrchestrator::handle_offer(const FabricSessionMessage& message) {
    if (!message.to_name.empty() && message.to_name != identity_.display_name) {
        return;
    }

    roster_.touch_peer(message.from_name, message.team, ReceiveStatus::Open, port_index_ == 0 ? 1 : 0);

    ReceiveStatus effective = identity_.receive_status;
    if (effective == ReceiveStatus::Busy) {
        effective = ReceiveStatus::Open;
    }

    if (effective == ReceiveStatus::Open) {
        listener_->pause();
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_.pending_offer = PendingOffer{message};
            state_.roster = roster_.peers();
        }
        accept_pending_offer();
        return;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.pending_offer = PendingOffer{message};
    state_.roster = roster_.peers();
    state_.status_message = "Incoming transfer from " + message.from_name;
    publish_state();
}

void TransferOrchestrator::accept_pending_offer() {
    PendingOffer offer_copy;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!state_.pending_offer) {
            return;
        }
        offer_copy = *state_.pending_offer;
        state_.pending_offer.reset();
    }

    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }

    payload_thread_ = std::thread([this, offer_copy]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!send_session_reply(offer_copy.message, SessionMessageKind::Accept)) {
            listener_->resume();
            finish_transfer(false, "Accept failed", state_.error_message);
            return;
        }
        run_inbound_payload(offer_copy.message);
    });
}

void TransferOrchestrator::decline_pending_offer() {
    PendingOffer offer_copy;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!state_.pending_offer) {
            return;
        }
        offer_copy = *state_.pending_offer;
        state_.pending_offer.reset();
    }

    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }

    payload_thread_ = std::thread([this, offer_copy]() {
        listener_->pause();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        send_session_reply(offer_copy.message, SessionMessageKind::Decline);
        listener_->resume();
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.status_message = "Declined transfer from " + offer_copy.message.from_name;
        publish_state();
    });
}

bool TransferOrchestrator::send_session_reply(const FabricSessionMessage& request,
                                              SessionMessageKind kind) {
    FabricSessionMessage reply{};
    reply.kind = kind;
    reply.session_id = request.session_id;
    reply.from_name = identity_.display_name;
    reply.team = identity_.team;
    reply.to_name = request.from_name;
    std::string error;
    const bool ok = send_session_with_routing(reply, true, &error);
    if (!ok) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.error_message = error;
        publish_state();
    }
    return ok;
}

void TransferOrchestrator::handle_accept(const FabricSessionMessage& message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!outbound_offer_ || outbound_offer_->session_id != message.session_id) {
        return;
    }
    state_.waiting_for_partner = true;
    state_.status_message = "Accepted — waiting for receiver to prepare…";
    awaiting_ready_ = true;
    publish_state();
}

void TransferOrchestrator::handle_decline(const FabricSessionMessage& message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!outbound_offer_ || outbound_offer_->session_id != message.session_id) {
        return;
    }
    if (staged_payload_.is_temp && !staged_payload_.path.empty()) {
        std::remove(staged_payload_.path.c_str());
    }
    staged_payload_ = {};
    outbound_offer_.reset();
    state_.busy = false;
    state_.waiting_for_partner = false;
    state_.status_message = "Transfer declined by " + message.from_name;
    state_.error_message = "Declined";
    publish_state();
}

void TransferOrchestrator::handle_ready(const FabricSessionMessage& message) {
    bool should_send = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!outbound_offer_ || outbound_offer_->session_id != message.session_id) {
            return;
        }
        should_send = awaiting_ready_;
    }
    if (should_send) {
        start_outbound_payload();
    }
}

void TransferOrchestrator::start_outbound_payload() {
    StagedPayload staged;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        staged = staged_payload_;
        state_.waiting_for_partner = false;
        state_.status_message = "Sending " + staged.display_name + "…";
    }
    publish_state();

    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }

    payload_thread_ = std::thread([this, staged]() {
        listener_->pause();
        ProgressCallback progress = [this](uint64_t done, uint64_t total, double elapsed) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_.bytes_done = done;
            state_.bytes_total = total;
            state_.live_mbps = (elapsed > 0.0 && done > 0)
                ? (static_cast<double>(done) / (1024.0 * 1024.0)) / elapsed
                : 0.0;
            publish_state();
        };
        TransferResult result =
            controller_->send_on_port(port_index_, staged.path, progress,
                                      payload_timeout_ms());
        listener_->resume();

        if (staged.is_temp) {
            std::remove(staged.path.c_str());
        }

        finish_transfer(result.ok,
                        result.ok ? "Send complete" : "Send failed",
                        result.error_message);
        outbound_offer_.reset();
        staged_payload_ = {};
        awaiting_ready_ = false;
    });
}

void TransferOrchestrator::run_inbound_payload(const FabricSessionMessage& offer) {
    listener_->pause();

    const std::string out_path =
        build_inbound_payload_path(identity_.receive_folder, offer.payload_name);

    std::string dir_error;
    ensure_parent_directories(out_path, &dir_error);

    // Signal readiness BEFORE receiving. Both the reply send and the payload
    // receive use the same per-port USB mutex, so they cannot overlap on one
    // device — sending ready first, then receiving, avoids a self-deadlock. The
    // sender only transmits the payload after it sees ready, and its payload
    // header uses a long timeout, so the receiver starting slightly later is safe.
    if (!send_session_reply(offer, SessionMessageKind::Ready)) {
        listener_->resume();
        finish_transfer(false, "Ready failed", state_.error_message);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.busy = true;
        state_.status_message = "Waiting for " + offer.from_name + " to send…";
    }
    publish_state();

    ProgressCallback progress = [this](uint64_t done, uint64_t total, double elapsed) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.bytes_done = done;
        state_.bytes_total = total;
        state_.live_mbps = (elapsed > 0.0 && done > 0)
            ? (static_cast<double>(done) / (1024.0 * 1024.0)) / elapsed
            : 0.0;
        publish_state();
    };
    TransferResult result = controller_->receive_on_port(
        port_index_, out_path, progress, usb_protocol::kPayloadHeaderTimeoutMs);

    if (!result.ok) {
        const FailedInboundReceive failed = handle_failed_inbound_receive(result, out_path);
        if (failed.cleanup.existed_before) {
            booth_log(port_index_,
                      "partial_cleanup",
                      out_path + " bytes=" + std::to_string(failed.cleanup.bytes_removed)
                          + (failed.cleanup.removed ? " removed" : " remove_failed"));
        }
        listener_->resume();
        finish_transfer(false, "Receive failed", failed.error_message);
        return;
    }

    if (offer.payload_type == "tar") {
        std::string extract_error;
        const std::string extract_dir = identity_.receive_folder;
        if (!extract_tar_to_dir(out_path, extract_dir, &extract_error)) {
            const PartialReceiveCleanup cleanup = cleanup_partial_receive_file(out_path);
            if (cleanup.existed_before) {
                booth_log(port_index_,
                          "partial_cleanup",
                          out_path + " tar_extract_fail bytes="
                              + std::to_string(cleanup.bytes_removed));
            }
            listener_->resume();
            finish_transfer(false, "Receive failed", extract_error);
            return;
        }
        std::remove(out_path.c_str());
    }

    listener_->resume();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.notification = "Received " + offer.payload_name + " from " + offer.from_name;
    }

    finish_transfer(true, "Receive complete → " + out_path, {});
}

void TransferOrchestrator::finish_transfer(bool ok,
                                           const std::string& message,
                                           const std::string& error) {
    booth_log(port_index_,
              ok ? "transfer_done" : "transfer_fail",
              message + (error.empty() ? "" : " err=" + error));
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.busy = false;
    state_.waiting_for_partner = false;
    state_.bytes_done = 0;
    state_.bytes_total = 0;
    state_.status_message = message;
    state_.error_message = ok ? "" : error;
    publish_state();
}

void TransferOrchestrator::run_loopback_test(const std::string& path) {
    if (is_busy()) {
        return;
    }
    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }
    payload_thread_ = std::thread([this, path]() {
        listener_->pause();
        TransferResult result = controller_->loopback_on_ports(path, 0, 1, nullptr);
        listener_->resume();
        finish_transfer(result.ok,
                        result.ok ? "Loopback verified" : "Loopback failed",
                        result.error_message);
    });
}
