#pragma once

#include "fabric_session_message.h"
#include "identity_profile.h"
#include "peer_roster.h"
#include "session_handshake.h"
#include "transfer_controller.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

enum class TransferDoneKind { None, Sent, Received };

struct PendingOffer {
    FabricSessionMessage message;
};

struct OrchestratorUiState {
    std::vector<PeerEntry> roster;
    IdentityProfile identity;
    bool busy = false;
    bool waiting_for_partner = false;
    std::string status_message;
    std::string error_message;
    std::optional<PendingOffer> pending_offer;
    uint64_t bytes_done = 0;
    uint64_t bytes_total = 0;
    double live_mbps = 0.0;
    double peak_mbps = 0.0;
    double result_mbps = 0.0;
    double booth_display_mib_s = 0.0;
    std::string transfer_label;
    std::string notification;
    std::string dev_log;
    bool fabric_connected = false;
    int fabric_devices_seen = 0;
    bool fabric_port_open = false;
    int fabric_port_index = 0;
    std::string fabric_device_label;
    int64_t last_announce_ms = 0;
    uint32_t fabric_activity_seq = 0;
};

class TransferOrchestrator {
public:
    using UiCallback = std::function<void(const OrchestratorUiState&)>;

    TransferOrchestrator(int port_index,
                         IdentityProfile identity,
                         UiCallback on_ui_update);
    ~TransferOrchestrator();

    TransferOrchestrator(const TransferOrchestrator&) = delete;
    TransferOrchestrator& operator=(const TransferOrchestrator&) = delete;

    void start(bool run_wiring_probe = true, bool enable_listener = true);
    void stop();

    bool is_busy() const;
    IdentityProfile identity() const;
    OrchestratorUiState snapshot() const;
    void set_identity(IdentityProfile identity);

    bool send_to_peer(const std::string& peer_name,
                      const std::vector<std::string>& paths,
                      const std::string& note);
    void accept_pending_offer();
    void decline_pending_offer();

    void run_loopback_test(const std::string& path);

    /** Clears the post-transfer Done panel (Ready idle state). */
    void dismiss_transfer_display();

    /** Clear transfer errors, cancel a stuck handshake, and resume listening. */
    void reset_connection();

private:
    void tick_presence();
    void start_presence_loop();
    void on_session_message(const FabricSessionMessage& message);
    void handle_offer(const FabricSessionMessage& message);
    void handle_accept(const FabricSessionMessage& message);
    void handle_decline(const FabricSessionMessage& message);
    void handle_ready(const FabricSessionMessage& message);
    void handle_announce(const FabricSessionMessage& message);
    void maybe_send_announce(int64_t now_ms);

    bool send_session_reply(const FabricSessionMessage& request,
                            SessionMessageKind kind);
    bool send_session_with_routing(const FabricSessionMessage& message,
                                   bool reverse_path,
                                   std::string* error_out);
    void run_inbound_payload(const FabricSessionMessage& offer);
    void start_outbound_payload();
    void finish_transfer(bool ok,
                         const std::string& message,
                         const std::string& error = {},
                         const TransferResult* result = nullptr,
                         TransferDoneKind done_kind = TransferDoneKind::None);
    void clear_outbound_session();
    void ensure_listener_active();
    void publish_state();
    ProgressCallback make_progress_callback();
    void begin_booth_display_rate();
    void ensure_wiring();
    void ensure_listener_started();
    void invalidate_dismiss();
    void schedule_dismiss_transfer_display();
    void bump_fabric_activity();

    struct StagedPayload {
        std::string path;
        std::string display_name;
        std::string payload_type;
        bool is_temp = false;
        uint32_t file_count = 1;
        uint64_t total_bytes = 0;
    };

    bool stage_paths(const std::vector<std::string>& paths,
                     const std::string& note,
                     StagedPayload& out,
                     std::string* error_out);

    int port_index_ = 0;
    IdentityProfile identity_;
    HandshakeTiming handshake_;
    PeerRoster roster_;
    UiCallback on_ui_update_;

    std::unique_ptr<TransferController> controller_;
    std::unique_ptr<class SessionListener> listener_;

    mutable std::mutex state_mutex_;
    OrchestratorUiState state_;

    std::atomic<bool> shutting_down_{false};
    std::thread startup_thread_;
    std::thread presence_thread_;
    std::thread payload_thread_;

    std::optional<FabricSessionMessage> active_offer_;
    std::optional<FabricSessionMessage> outbound_offer_;
    StagedPayload staged_payload_{};
    std::string pending_receive_path_;
    bool awaiting_ready_ = false;
    bool wiring_checked_ = false;
    bool listener_started_ = false;
    int working_send_port_ = 1;
    int working_recv_port_ = 0;
    bool fabric_was_connected_ = false;
    int64_t last_announce_ms_ = 0;

    std::string instance_id_;

    std::atomic<uint64_t> dismiss_epoch_{0};

    // Presence-probe throttle (presence thread only).
    int64_t last_probe_ms_ = 0;
    bool cached_port_ok_ = false;
    int cached_devices_ = 0;
};
