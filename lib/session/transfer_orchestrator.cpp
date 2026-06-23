#include "transfer_orchestrator.h"

#include "platform_util.h"
#include "session_handshake.h"
#include "announce_note.h"
#include "booth_log.h"
#include "booth_display.h"
#include "fabric_link_policy.h"
#include "fabric_meta_file.h"
#include "fabric_port.h"
#include "fabric_tar_pack.h"
#include "link_status.h"
#include "receive_payload.h"
#include "session_listener.h"
#include "usb_protocol.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

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

class ListenerPauseGuard {
public:
    explicit ListenerPauseGuard(SessionListener* listener)
        : listener_(listener) {
        if (listener_) {
            listener_->pause();
        }
    }

    ~ListenerPauseGuard() {
        if (listener_) {
            listener_->resume();
        }
    }

    ListenerPauseGuard(const ListenerPauseGuard&) = delete;
    ListenerPauseGuard& operator=(const ListenerPauseGuard&) = delete;

private:
    SessionListener* listener_;
};

bool looks_like_done_display(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    return text.find("Sent at") != std::string::npos
           || text.find("Received at") != std::string::npos
           || text == "Sent" || text == "Received";
}

}  // namespace

namespace {

void load_shared_booth_path(int& send_port, int& recv_port) {
    std::ifstream file(platform::shared_booth_path_config());
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
    , handshake_(handshake_timing_from_identity(identity_))
    , on_ui_update_(std::move(on_ui_update))
    , instance_id_(make_instance_id()) {
    state_.identity = identity_;
    state_.booth_display_mib_s = identity_.booth_display_mib_s;
    if (identity_.transfer_timeout_ms > 0) {
        set_payload_timeout_ms(static_cast<unsigned>(identity_.transfer_timeout_ms));
    }
    if (identity_.usb_inflight_mb > 0) {
        set_inflight_budget_mb(static_cast<unsigned>(identity_.usb_inflight_mb));
    }
    booth_log(port_index_, "usb_inflight",
              "usbfs_limit_mb=" + std::to_string(usbfs_limit_mb())
                  + " queue_depth=" + std::to_string(inflight_queue_depth()));
    if (identity_.booth_display_mib_s > 0.0) {
        booth_log(port_index_, "booth_display",
                  "base_mib_s=" + std::to_string(identity_.booth_display_mib_s)
                      + " jitter_pct=" + std::to_string(identity_.booth_display_jitter_pct));
    }
    roster_.seed_from_config(identity_.peers);
    state_.roster = roster_.peers();

    controller_ = std::make_unique<TransferController>(port_index_, nullptr);

    // Announces run from tick_presence(), not before each USB read — the fabric
    // does not buffer session messages; blocking on OUT for announce drops offers.
    listener_ = std::make_unique<SessionListener>(
        controller_.get(),
        port_index_,
        [this](const FabricSessionMessage& message) { on_session_message(message); });
    listener_->set_session_header_timeout_ms(handshake_.session_header_timeout_ms);
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
    if (controller_) {
        const int leg = controller_->fabric_leg();
        const std::string serial = controller_->fabric_device_serial();
        booth_log(leg, "cable_serial", serial.empty() ? "(none)" : serial);
        booth_log(leg,
                  "listener_started",
                  "working_send=" + std::to_string(working_send_port_)
                      + " working_recv=" + std::to_string(working_recv_port_));
    } else {
        booth_log(port_index_,
                  "listener_started",
                  "working_send=" + std::to_string(working_send_port_)
                      + " working_recv=" + std::to_string(working_recv_port_));
    }
}

void TransferOrchestrator::stop() {
    shutting_down_.store(true, std::memory_order_release);
    invalidate_dismiss();
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
    return state_.busy || accepting_inbound_session_id_.has_value();
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
        handshake_ = handshake_timing_from_identity(identity_);
        state_.identity = identity_;
        state_.booth_display_mib_s = identity_.booth_display_mib_s;
        if (identity_.transfer_timeout_ms > 0) {
            set_payload_timeout_ms(static_cast<unsigned>(identity_.transfer_timeout_ms));
        }
        if (identity_.usb_inflight_mb > 0) {
            set_inflight_budget_mb(static_cast<unsigned>(identity_.usb_inflight_mb));
        }
        roster_.seed_from_config(identity_.peers);
        state_.roster = roster_.peers();
        last_announce_ms_ = 0;
    }
    save_identity_profile(identity_);
    publish_state();
}

void TransferOrchestrator::bump_fabric_activity() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++state_.fabric_activity_seq;
    }
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
    out.path = create_tar_for_paths(paths, &tar_error);
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
    if (ok) {
        bump_fabric_activity();
    }
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
        invalidate_dismiss();
        state_.busy = true;
        state_.waiting_for_partner = true;
        state_.transfer_label.clear();
        state_.bytes_total = staged.total_bytes;
        state_.bytes_done = 0;
        state_.peak_mbps = 0.0;
        state_.result_mbps = 0.0;
        state_.status_message = "Sending offer to " + peer->display_name + "…";
        state_.error_message.clear();
    }
    publish_state();

    payload_thread_ = std::thread([this, staged, offer, peer]() {
        {
            ListenerPauseGuard listener_guard(listener_.get());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::string send_error;
            const bool sent = send_session_with_routing(offer, false, &send_error);

            if (!sent) {
                if (staged.is_temp) {
                    std::remove(staged.path.c_str());
                }
                finish_transfer(false, "Send failed", send_error);
                return;
            }
            // Keep USB IN free so the receiver can read the offer (fabric does not buffer).
            std::this_thread::sleep_for(
                std::chrono::milliseconds(handshake_.accept_ready_gap_ms));
        }
        ensure_listener_started();
        ensure_listener_active();
        if (listener_) {
            listener_->set_tight_poll(true);
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            staged_payload_ = staged;
            outbound_offer_ = offer;
            awaiting_ready_ = false;
            state_.waiting_for_partner = true;
            state_.transfer_label = staged.display_name;
            state_.bytes_total = staged.total_bytes;
            state_.peak_mbps = 0.0;
            state_.result_mbps = 0.0;
            state_.status_message = "Waiting for " + peer->display_name
                + " to accept… (" + std::to_string(handshake_.accept_timeout_sec) + "s)";
            state_.error_message.clear();
        }
        publish_state();

        const auto now0 = std::chrono::steady_clock::now();
        const auto accept_deadline = now0 + std::chrono::seconds(handshake_.accept_timeout_sec);
        const auto retransmit_at = now0 + std::chrono::milliseconds(
            handshake_.accept_reply_delay_ms + handshake_.accept_ready_gap_ms + 150);
        bool offer_retransmitted = false;
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

            if (!accepted && !offer_retransmitted && now >= retransmit_at) {
                offer_retransmitted = true;
                {
                    ListenerPauseGuard listener_guard(listener_.get());
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    std::string send_error;
                    if (send_session_with_routing(offer, false, &send_error)) {
                        booth_log(port_index_, "offer_retransmit", "to=" + peer->display_name);
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(handshake_.accept_ready_gap_ms));
                    }
                }
                ensure_listener_active();
            }

            if (!accepted) {
                if (now >= accept_deadline) {
                    if (staged.is_temp) {
                        std::remove(staged.path.c_str());
                    }
                    outbound_offer_.reset();
                    staged_payload_ = {};
                    awaiting_ready_ = false;
                    finish_transfer(false,
                                    "Timed out",
                                    "No response from " + peer->display_name
                                        + " within "
                                        + std::to_string(handshake_.accept_timeout_sec) + "s");
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
                    ready_deadline = now + std::chrono::seconds(handshake_.ready_timeout_sec);
                }
                if (now >= ready_deadline) {
                    if (staged.is_temp) {
                        std::remove(staged.path.c_str());
                    }
                    outbound_offer_.reset();
                    staged_payload_ = {};
                    awaiting_ready_ = false;
                    finish_transfer(false,
                                    "Timed out",
                                    peer->display_name
                                        + " accepted but did not start receiving in time");
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (listener_) {
            listener_->set_tight_poll(false);
        }
    });
    return true;
}

void TransferOrchestrator::on_session_message(const FabricSessionMessage& message) {
    bump_fabric_activity();
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
            handle_announce(message);
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
                            "Device disconnected",
                            "USB cable unplugged or power change");
        }
        publish_state();
    }

    const bool fabric_connected = fabric_link::fabric_connected_from_enumeration(port_ok);
    const bool fabric_just_connected = fabric_connected && !fabric_was_connected_;

    if (!fabric_connected) {
        roster_.set_all_peers_offline();
    } else {
        roster_.mark_stale_peers_offline(std::chrono::seconds(45));
        if (fabric_just_connected) {
            last_announce_ms_ = 0;
        }
    }

    fabric_was_connected_ = port_ok;

    const auto roster = roster_.peers();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.fabric_connected = fabric_connected;
        state_.fabric_devices_seen = devices_seen;
        state_.fabric_port_open = port_ok;
        state_.fabric_port_index = controller_ ? controller_->fabric_leg() : port_index_;
        if (!fabric_connected) {
            state_.fabric_device_label.clear();
        } else {
            const std::string label = controller_->fabric_device_label();
            if (!label.empty()) {
                state_.fabric_device_label = label;
            }
        }
        state_.roster = roster;
        state_.last_announce_ms = last_announce_ms_;
    }
    publish_state();
    maybe_send_announce(now);
}

void TransferOrchestrator::handle_announce(const FabricSessionMessage& message) {
    if (message.from_name.empty()) {
        return;
    }

    const int my_leg = controller_ ? controller_->fabric_leg() : port_index_;
    int announced_port = default_remote_guess_leg(my_leg);
    ReceiveStatus peer_status = ReceiveStatus::AskFirst;
    std::string instance_id;
    parse_announce_note(message.note, announced_port, &announced_port, &peer_status, &instance_id);

    if (!instance_id.empty() && instance_id == instance_id_) {
        booth_log(my_leg, "announce_echo_reject", "instance=" + instance_id);
        return;
    }
    if (instance_id.empty() && message.from_name == identity_.display_name
        && announced_port == my_leg) {
        booth_log(my_leg, "announce_echo_reject", "legacy name=" + message.from_name);
        return;
    }

    const int peer_port = resolve_remote_fabric_port(my_leg, announced_port);
    if (peer_port < 0) {
        booth_log(my_leg,
                  "announce_echo_reject",
                  message.from_name + " port=" + std::to_string(display_port_from_leg(announced_port)));
        return;
    }
    roster_.touch_peer(message.from_name, message.team, peer_status, peer_port, instance_id);
    booth_log(my_leg,
              "announce_received",
              message.from_name + " port=" + std::to_string(display_port_from_leg(peer_port))
                  + " note=" + message.note);

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.roster = roster_.peers();
    }
    publish_state();
}

void TransferOrchestrator::clear_outbound_session() {
    outbound_offer_.reset();
    staged_payload_ = {};
    awaiting_ready_ = false;
    active_offer_.reset();
}

void TransferOrchestrator::ensure_listener_active() {
    if (listener_) {
        listener_->resume();
    }
}

void TransferOrchestrator::maybe_send_announce(int64_t now_ms) {
    if (identity_.display_name.empty()) {
        return;
    }
    if (listener_ && listener_->is_paused()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        // While waiting for accept/ready, do not send announces — they use the
        // same ROCKETBX payload header wire format and the receiver may
        // mistake them for the incoming file (typically ~100–200 B).
        if (outbound_offer_) {
            return;
        }
    }

    bool fabric_connected = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        fabric_connected = state_.fabric_connected;
    }
    if (!fabric_connected) {
        return;
    }

    constexpr int64_t kAnnounceIntervalMs = 15000;
    if (last_announce_ms_ != 0 && now_ms - last_announce_ms_ < kAnnounceIntervalMs) {
        return;
    }

    FabricSessionMessage message{};
    message.kind = SessionMessageKind::Announce;
    message.from_name = identity_.display_name;
    message.team = identity_.team;
    message.session_id = make_session_id();
    const int my_leg = controller_ ? controller_->fabric_leg() : port_index_;
    message.note = build_announce_note(my_leg, identity_.receive_status, instance_id_);

    std::string error;
    if (send_session_with_routing(message, false, &error)) {
        last_announce_ms_ = now_ms;
        booth_log(port_index_,
                  "announce_sent",
                  "from=" + message.from_name + " note=" + message.note);
    } else if (!error.empty()) {
        booth_log(port_index_, "announce_fail", error);
    }
}

void TransferOrchestrator::handle_offer(const FabricSessionMessage& message) {
    if (!message.to_name.empty() && message.to_name != identity_.display_name) {
        booth_log(port_index_,
                  "offer_rejected",
                  "to=" + message.to_name + " local=" + identity_.display_name + " from="
                      + message.from_name);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (accepting_inbound_session_id_
            && *accepting_inbound_session_id_ == message.session_id) {
            booth_log(port_index_, "offer_dup_ignore", "accepting session=" + message.session_id);
            return;
        }
        if (last_completed_inbound_session_id_
            && *last_completed_inbound_session_id_ == message.session_id) {
            booth_log(port_index_, "offer_completed_ignore", "session=" + message.session_id);
            return;
        }
        if (state_.pending_offer
            && state_.pending_offer->message.session_id == message.session_id) {
            booth_log(port_index_, "offer_dup_pending", "session=" + message.session_id);
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.error_message.clear();
    }

    if (is_busy()) {
        booth_log(port_index_, "offer_rejected", "busy from=" + message.from_name);
        return;
    }

    ReceiveStatus effective = identity_.receive_status;
    if (effective == ReceiveStatus::Busy) {
        effective = ReceiveStatus::Open;
    }

    if (effective == ReceiveStatus::Open) {
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
        if (accepting_inbound_session_id_
            && *accepting_inbound_session_id_ == offer_copy.message.session_id) {
            booth_log(port_index_,
                      "accept_inflight_ignore",
                      "session=" + offer_copy.message.session_id);
            return;
        }
        if (last_completed_inbound_session_id_
            && *last_completed_inbound_session_id_ == offer_copy.message.session_id) {
            booth_log(port_index_,
                      "accept_completed_ignore",
                      "session=" + offer_copy.message.session_id);
            state_.pending_offer.reset();
            return;
        }
        state_.pending_offer.reset();
        accepting_inbound_session_id_ = offer_copy.message.session_id;
    }

    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }

    payload_thread_ = std::thread([this, offer_copy]() {
        if (last_completed_inbound_session_id_
            && *last_completed_inbound_session_id_ == offer_copy.message.session_id) {
            booth_log(port_index_,
                      "inbound_dup_skip",
                      "session=" + offer_copy.message.session_id);
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                accepting_inbound_session_id_.reset();
            }
            return;
        }

        ListenerPauseGuard listener_guard(listener_.get());
        std::this_thread::sleep_for(
            std::chrono::milliseconds(handshake_.accept_reply_delay_ms));
        if (!send_session_reply(offer_copy.message, SessionMessageKind::Accept)) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                accepting_inbound_session_id_.reset();
            }
            finish_transfer(false, "Accept failed", state_.error_message);
            return;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(handshake_.accept_ready_gap_ms));
        run_inbound_payload(offer_copy.message);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            accepting_inbound_session_id_.reset();
        }
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
        ListenerPauseGuard listener_guard(listener_.get());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        send_session_reply(offer_copy.message, SessionMessageKind::Decline);
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
    StagedPayload staged;
    std::string from_name;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!outbound_offer_ || outbound_offer_->session_id != message.session_id) {
            return;
        }
        from_name = message.from_name;
        staged = staged_payload_;
        clear_outbound_session();
    }
    if (staged.is_temp && !staged.path.empty()) {
        std::remove(staged.path.c_str());
    }
    finish_transfer(false, "Transfer declined by " + from_name, "Declined");
}

void TransferOrchestrator::handle_ready(const FabricSessionMessage& message) {
    bool should_send = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!outbound_offer_ || outbound_offer_->session_id != message.session_id) {
            return;
        }
        if (!awaiting_ready_) {
            // Accept may have been dropped on the no-buffer fabric; ready implies acceptance.
            awaiting_ready_ = true;
            state_.status_message = "Accepted — waiting for receiver to prepare…";
            publish_state();
        }
        should_send = awaiting_ready_;
    }
    if (should_send) {
        start_outbound_payload();
    }
}

void TransferOrchestrator::start_outbound_payload() {
    // handle_ready runs on the session listener thread; pause synchronously so
    // listen_loop cannot immediately start another USB read and race send_on_port.
    if (listener_) {
        listener_->pause();
    }

    StagedPayload staged;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        staged = staged_payload_;
        state_.waiting_for_partner = false;
        state_.transfer_label = staged.display_name;
        state_.peak_mbps = 0.0;
        state_.result_mbps = 0.0;
        state_.bytes_done = 0;
        state_.bytes_total = staged.total_bytes;
        state_.status_message = "Sending " + staged.display_name + "…";
        begin_booth_display_rate();
    }
    publish_state();

    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }

    payload_thread_ = std::thread([this, staged]() {
        ListenerPauseGuard listener_guard(listener_.get());
        TransferResult result =
            controller_->send_on_port(port_index_, staged.path, make_progress_callback(),
                                      payload_timeout_ms());

        if (staged.is_temp) {
            std::remove(staged.path.c_str());
        }

        finish_transfer(result.ok,
                        result.ok ? "Send complete" : "Send failed",
                        result.error_message,
                        &result,
                        result.ok ? TransferDoneKind::Sent : TransferDoneKind::None);
    });
}

void TransferOrchestrator::run_inbound_payload(const FabricSessionMessage& offer) {
    ListenerPauseGuard listener_guard(listener_.get());

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
        finish_transfer(false, "Ready failed", state_.error_message);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        invalidate_dismiss();
        state_.busy = true;
        state_.transfer_label = offer.payload_name;
        state_.peak_mbps = 0.0;
        state_.result_mbps = 0.0;
        state_.bytes_done = 0;
        if (offer.total_bytes > 0) {
            state_.bytes_total = offer.total_bytes;
        }
        state_.status_message = "Receiving " + offer.payload_name + "…";
        begin_booth_display_rate();
    }
    publish_state();

    TransferResult result = controller_->receive_on_port(
        port_index_, out_path, make_progress_callback(), handshake_.payload_header_timeout_ms);

    if (!result.ok) {
        const FailedInboundReceive failed = handle_failed_inbound_receive(result, out_path);
        if (failed.cleanup.existed_before) {
            booth_log(port_index_,
                      "partial_cleanup",
                      out_path + " bytes=" + std::to_string(failed.cleanup.bytes_removed)
                          + (failed.cleanup.removed ? " removed" : " remove_failed"));
        }
        finish_transfer(false, "Receive failed", failed.error_message);
        return;
    }

    if (offer.total_bytes > 0 && result.bytes_transferred != offer.total_bytes) {
        const PartialReceiveCleanup cleanup = cleanup_partial_receive_file(out_path);
        if (cleanup.existed_before) {
            booth_log(port_index_,
                      "partial_cleanup",
                      out_path + " size_mismatch bytes="
                          + std::to_string(cleanup.bytes_removed));
        }
        finish_transfer(false,
                        "Receive failed",
                        "Expected " + std::to_string(offer.total_bytes) + " bytes, got "
                            + std::to_string(result.bytes_transferred));
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
            finish_transfer(false, "Receive failed", extract_error);
            return;
        }
        std::remove(out_path.c_str());
    }

    finish_transfer(true,
                    "Receive complete",
                    {},
                    &result,
                    TransferDoneKind::Received);
}

ProgressCallback TransferOrchestrator::make_progress_callback() {
    return [this](uint64_t done, uint64_t total, double elapsed) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.bytes_done = done;
        state_.bytes_total = total;
        const double live = (elapsed > 0.0 && done > 0)
            ? (static_cast<double>(done) / (1024.0 * 1024.0)) / elapsed
            : 0.0;
        if (state_.booth_display_mib_s > 0.0) {
            state_.live_mbps = state_.booth_display_mib_s;
            if (state_.peak_mbps < state_.booth_display_mib_s) {
                state_.peak_mbps = state_.booth_display_mib_s;
            }
        } else {
            state_.live_mbps = live;
            if (live > state_.peak_mbps) {
                state_.peak_mbps = live;
            }
        }
        publish_state();
    };
}

void TransferOrchestrator::begin_booth_display_rate() {
    if (identity_.booth_display_mib_s <= 0.0) {
        state_.booth_display_mib_s = 0.0;
        return;
    }
    state_.booth_display_mib_s = roll_booth_display_mib_s(identity_.booth_display_mib_s,
                                                        identity_.booth_display_jitter_pct);
    state_.live_mbps = state_.booth_display_mib_s;
    booth_log(port_index_, "booth_display",
              "ui_rate_mib_s=" + std::to_string(state_.booth_display_mib_s)
                  + " base_mib_s=" + std::to_string(identity_.booth_display_mib_s)
                  + " jitter_pct=" + std::to_string(identity_.booth_display_jitter_pct));
}

void TransferOrchestrator::finish_transfer(bool ok,
                                           const std::string& message,
                                           const std::string& error,
                                           const TransferResult* result,
                                           TransferDoneKind done_kind) {
    const bool schedule_dismiss =
        ok && (done_kind == TransferDoneKind::Sent || done_kind == TransferDoneKind::Received);
    booth_log(port_index_,
              ok ? "transfer_done" : "transfer_fail",
              message + (error.empty() ? "" : " err=" + error));
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        clear_outbound_session();
        state_.busy = false;
        state_.waiting_for_partner = false;
        state_.status_message = message;
        state_.error_message = ok ? "" : error;
        state_.live_mbps = 0.0;

        if (ok) {
            if (result != nullptr) {
                const uint64_t transferred = result->bytes_transferred;
                const uint64_t expected = result->expected_bytes > 0
                                              ? result->expected_bytes
                                              : state_.bytes_total;
                if (expected > 0) {
                    state_.bytes_total = expected;
                }
                if (transferred > 0) {
                    state_.bytes_done = transferred;
                } else if (expected > 0) {
                    state_.bytes_done = expected;
                }

                if (state_.booth_display_mib_s > 0.0) {
                    state_.result_mbps = state_.booth_display_mib_s;
                    state_.peak_mbps = state_.booth_display_mib_s;
                } else {
                    const uint64_t speed_bytes =
                        transferred > 0 ? transferred : expected;
                    double measured = result->mbps;
                    if (measured <= 0.0 && speed_bytes > 0 && result->seconds > 0.0) {
                        const double effective_seconds = std::max(result->seconds, 0.001);
                        measured = (static_cast<double>(speed_bytes) / (1024.0 * 1024.0))
                                   / effective_seconds;
                    }
                    if (measured > 0.0) {
                        state_.result_mbps = measured;
                        if (state_.peak_mbps < measured) {
                            state_.peak_mbps = measured;
                        }
                    } else if (state_.peak_mbps > 0.0) {
                        state_.result_mbps = state_.peak_mbps;
                    }
                }
            } else if (state_.bytes_total > 0) {
                state_.bytes_done = state_.bytes_total;
                if (state_.booth_display_mib_s > 0.0) {
                    state_.result_mbps = state_.booth_display_mib_s;
                    state_.peak_mbps = state_.booth_display_mib_s;
                } else if (state_.peak_mbps > 0.0) {
                    state_.result_mbps = state_.peak_mbps;
                }
            }

            if (done_kind == TransferDoneKind::Sent
                || done_kind == TransferDoneKind::Received) {
                const std::string done_msg = transfer_done_message(
                    done_kind == TransferDoneKind::Sent, state_.result_mbps);
                state_.status_message = done_msg;
                state_.notification = done_msg;
            }
            if (done_kind == TransferDoneKind::Received && accepting_inbound_session_id_) {
                last_completed_inbound_session_id_ = *accepting_inbound_session_id_;
            }
        } else {
            state_.bytes_done = 0;
            state_.bytes_total = 0;
            state_.peak_mbps = 0.0;
            state_.result_mbps = 0.0;
            state_.transfer_label.clear();
            last_announce_ms_ = 0;
            state_.last_announce_ms = 0;
        }
        publish_state();
    }
    if (listener_) {
        listener_->set_tight_poll(false);
    }
    ensure_listener_active();
    if (schedule_dismiss) {
        schedule_dismiss_transfer_display();
    }
}

void TransferOrchestrator::invalidate_dismiss() {
    dismiss_epoch_.fetch_add(1, std::memory_order_relaxed);
}

void TransferOrchestrator::reset_connection() {
    invalidate_dismiss();
    booth_log(port_index_, "reset_connection", "user");

    StagedPayload staged{};
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        staged = staged_payload_;
        state_.waiting_for_partner = false;
        state_.busy = false;
        state_.error_message.clear();
        state_.status_message.clear();
        state_.notification.clear();
        state_.bytes_done = 0;
        state_.bytes_total = 0;
        state_.transfer_label.clear();
        state_.peak_mbps = 0.0;
        state_.result_mbps = 0.0;
        state_.live_mbps = 0.0;
        state_.booth_display_mib_s = identity_.booth_display_mib_s;
        state_.pending_offer.reset();
    }

    accepting_inbound_session_id_.reset();
    last_completed_inbound_session_id_.reset();
    awaiting_ready_ = false;
    clear_outbound_session();
    staged_payload_ = {};

    if (staged.is_temp && !staged.path.empty()) {
        std::remove(staged.path.c_str());
    }

    if (listener_) {
        listener_->resume();
    }

    if (payload_thread_.joinable()) {
        std::atomic<bool> joined{false};
        std::thread reaper([this, &joined] {
            payload_thread_.join();
            joined.store(true, std::memory_order_release);
        });
        for (int i = 0; i < 40 && !joined.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (joined.load(std::memory_order_acquire)) {
            if (reaper.joinable()) {
                reaper.join();
            }
        } else {
            booth_log(port_index_, "reset_connection", "payload thread detach on timeout");
            if (reaper.joinable()) {
                reaper.detach();
            }
            payload_thread_.detach();
        }
    }

    ensure_listener_active();
    publish_state();
}

void TransferOrchestrator::dismiss_transfer_display() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_.busy) {
            return;
        }
        if (!looks_like_done_display(state_.notification)
            && !looks_like_done_display(state_.status_message)) {
            return;
        }
        state_.notification.clear();
        state_.status_message.clear();
        state_.bytes_done = 0;
        state_.bytes_total = 0;
        state_.transfer_label.clear();
        state_.peak_mbps = 0.0;
        state_.result_mbps = 0.0;
        state_.live_mbps = 0.0;
    }
    publish_state();
}

void TransferOrchestrator::schedule_dismiss_transfer_display() {
    const uint64_t epoch = dismiss_epoch_.fetch_add(1, std::memory_order_relaxed) + 1;
    std::thread([this, epoch]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(kTransferDoneDismissMs));
        if (shutting_down_.load(std::memory_order_acquire)) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (epoch != dismiss_epoch_.load(std::memory_order_relaxed) || state_.busy) {
                return;
            }
            if (!looks_like_done_display(state_.notification)
                && !looks_like_done_display(state_.status_message)) {
                return;
            }
            state_.notification.clear();
            state_.status_message.clear();
            state_.bytes_done = 0;
            state_.bytes_total = 0;
            state_.transfer_label.clear();
            state_.peak_mbps = 0.0;
            state_.result_mbps = 0.0;
            state_.live_mbps = 0.0;
        }
        publish_state();
    }).detach();
}

void TransferOrchestrator::run_loopback_test(const std::string& path) {
    if (is_busy()) {
        return;
    }
    if (payload_thread_.joinable()) {
        payload_thread_.join();
    }
    payload_thread_ = std::thread([this, path]() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_.transfer_label = basename_of(path);
            state_.peak_mbps = 0.0;
            state_.result_mbps = 0.0;
            begin_booth_display_rate();
        }
        listener_->pause();
        TransferResult result =
            controller_->loopback_on_ports(path, 0, 1, make_progress_callback());
        listener_->resume();
        finish_transfer(result.ok,
                        result.ok ? "Loopback verified" : "Loopback failed",
                        result.error_message,
                        &result);
    });
}
