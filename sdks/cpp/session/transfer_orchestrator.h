#pragma once

#include "session_message.h"
#include "identity_profile.h"
#include "peer_roster.h"
#include "session_handshake.h"

#include "rocketbox/sdk.h"
#include "usb_transfer.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

enum class TransferDoneKind { None, Sent, Received };

/** Peer-card link icon: absent / attempting / established (both sides). */
enum class LinkUiState { None, Linking, Linked };

struct PendingOffer {
    SessionMessage message;
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
    double display_rate_mib_s = 0.0;
    std::string transfer_label;
    std::string notification;
    std::string dev_log;
    bool usb_connected = false;
    int devices_seen = 0;
    bool usb_port_open = false;
    int usb_port_index = 0;
    std::string device_label;
    int64_t last_announce_ms = 0;
    uint32_t usb_activity_seq = 0;
    int linked_port = 0;
    LinkUiState link_state = LinkUiState::None;
};

/** Session + transfer orchestration over a RocketBoxTransport (SDK). */
class SessionOrchestrator {
public:
    using UiCallback = std::function<void(const OrchestratorUiState&)>;

    SessionOrchestrator(std::shared_ptr<rocketbox::RocketBoxTransport> transport,
                        IdentityProfile identity,
                        UiCallback on_ui_update);
    ~SessionOrchestrator();

    SessionOrchestrator(const SessionOrchestrator&) = delete;
    SessionOrchestrator& operator=(const SessionOrchestrator&) = delete;

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
    void dismiss_transfer_display();
    void reset_connection();
    void request_announce();
    void release_link();

private:
    void tick_presence();
    void start_presence_loop();
    void on_session_message(const SessionMessage& message);
    void handle_offer(const SessionMessage& message);
    void handle_accept(const SessionMessage& message);
    void handle_decline(const SessionMessage& message);
    void handle_ready(const SessionMessage& message);
    void handle_announce(const SessionMessage& message);
    void maybe_send_announce(int64_t now_ms, bool force = false);
    int log_leg() const;

    bool send_session_reply(const SessionMessage& request, SessionMessageKind kind);
    bool send_session_with_routing(const SessionMessage& message,
                                   bool reverse_path,
                                   std::string* error_out);
    void run_inbound_payload(const SessionMessage& offer);
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
    void begin_display_rate();
    void ensure_wiring();
    void ensure_listener_started();
    void invalidate_dismiss();
    void schedule_dismiss_transfer_display();
    void bump_usb_activity();
    void note_peer_alive(const std::string& display_name);

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

    std::shared_ptr<rocketbox::RocketBoxTransport> transport_;
    int port_index_ = 0;
    IdentityProfile identity_;
    HandshakeTiming handshake_;
    PeerRoster roster_;
    UiCallback on_ui_update_;
    std::unique_ptr<class SessionListener> listener_;

    mutable std::mutex state_mutex_;
    OrchestratorUiState state_;

    std::atomic<bool> shutting_down_{false};
    std::thread startup_thread_;
    std::thread presence_thread_;
    std::thread payload_thread_;

    std::optional<SessionMessage> active_offer_;
    std::optional<SessionMessage> outbound_offer_;
    StagedPayload staged_payload_{};
    std::string pending_receive_path_;
    bool awaiting_ready_ = false;
    bool wiring_checked_ = false;
    bool listener_started_ = false;
    int working_send_port_ = 1;
    int working_recv_port_ = 0;
    bool usb_was_connected_ = false;
    int64_t last_announce_ms_ = 0;
    std::mutex announce_mutex_;
    int announce_rotate_index_ = 0;
    std::string instance_id_;
    std::string session_peer_name_;
    std::optional<std::string> accepting_inbound_session_id_;
    std::optional<std::string> last_completed_inbound_session_id_;
    std::atomic<uint64_t> dismiss_epoch_{0};
    int64_t last_probe_ms_ = 0;
    bool cached_port_ok_ = false;
    int cached_devices_ = 0;
};

using TransferOrchestrator = SessionOrchestrator;
