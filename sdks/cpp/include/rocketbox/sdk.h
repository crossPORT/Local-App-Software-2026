#pragma once

#include "rocketbox/transfer_ops.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rocketbox {

struct SystemInfo {
    std::string id;
    std::string name;
    std::string status;  // "reachable", "busy", "offline"
};

struct ParsedHeader {
    uint64_t file_size = 0;
    std::string filename;
    uint8_t frame_kind = 0;
};

enum class TransportMode { Usb, Sim };
enum class ListenMode { Always, Handshake, Off };
enum class AnnouncePresenceMode { Burst, Rotate };

/** App-layer transport: link + session + data + native file/switch ops. */
class RocketBoxTransport {
public:
    virtual ~RocketBoxTransport() = default;

    // Link
    virtual bool connected() const = 0;
    /** Libusb sort index used to open the device (I/O). */
    virtual int port_index() const = 0;
    /** Serial-derived leg 0–3 (announce / UI). Same as former fabric_leg(). */
    virtual int resolved_port_index() const { return port_index(); }
    /** Silkscreen Port 1–4 from serial-derived leg. */
    virtual int display_port() const { return resolved_port_index() + 1; }
    virtual std::string serial() const = 0;
    virtual std::string system_id() const = 0;
    virtual std::string describe_device() const = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void reset_connection() = 0;

    // Session
    virtual void set_listen_mode(ListenMode mode) = 0;
    virtual void ensure_listening() = 0;
    virtual void sync_systems(std::function<void(const std::vector<SystemInfo>&)> handler) = 0;
    virtual void ensure_circuit(const std::string& peer_system_id) = 0;
    virtual void clear_circuit() = 0;
    virtual int switch_dest() const = 0;
    virtual void send_session_message(const std::vector<uint8_t>& message) = 0;
    virtual void send_announce_presence(const std::vector<uint8_t>& message,
                                        AnnouncePresenceMode mode) = 0;
    virtual bool try_receive_session_message(unsigned header_timeout_ms,
                                             std::vector<uint8_t>* out) = 0;

    // Data (ROCKETBX bytes)
    virtual void send_bytes(const std::vector<uint8_t>& payload,
                            const std::string& filename = {}) = 0;
    virtual ParsedHeader receive_header() = 0;
    virtual std::vector<uint8_t> receive_payload(uint64_t file_size) = 0;
    virtual std::vector<uint8_t> receive_bytes() = 0;
    virtual void prepare_for_payload_send() = 0;
    virtual void wait_for_idle() = 0;
    virtual void on_data_message(std::function<void(const std::vector<uint8_t>&)> cb) {
        (void)cb;
    }

    // Native engine ops (SessionOrchestrator / SessionListener)
    virtual int device_count() const { return connected() ? 1 : 0; }
    virtual bool port_available() const { return connected(); }
    virtual void request_shutdown() { disconnect(); }
    virtual bool is_shutting_down() const { return !connected(); }
    virtual bool is_transfer_busy() const { return false; }
    virtual FileTransferResult switch_port(int dest_port);
    virtual FileTransferResult switch_port_if_needed(int dest_port);
    virtual void mark_switch_preserve() {}
    virtual bool switch_preserve() const { return false; }
    virtual FileTransferResult send_file_on_port(int port,
                                                 const std::string& path,
                                                 FileProgressFn progress,
                                                 unsigned timeout_ms,
                                                 uint8_t frame_kind);
    virtual FileTransferResult receive_file_on_port(int port,
                                                    const std::string& path,
                                                    FileProgressFn progress,
                                                    unsigned header_timeout_ms,
                                                    uint8_t expected_frame_kind);
    virtual FileTransferResult loopback_files(const std::string& path,
                                              int send_port,
                                              int recv_port,
                                              FileProgressFn progress);
};

std::unique_ptr<RocketBoxTransport> create_rocketbox_transport(TransportMode mode,
                                                               int display_port = 1);

}  // namespace rocketbox
