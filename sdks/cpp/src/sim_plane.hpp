#pragma once

#include "rocketbox/sdk.h"
#include "rocketbox_frame.hpp"
#include "sim_net.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rocketbox {
namespace detail {

/** Sim backend: TCP :1772 EP-mux to simulated-hardware (switch on EP4, data on EP1/2). */
class SimPlane final : public RocketBoxTransport {
public:
    explicit SimPlane(int display_port);
    ~SimPlane() override;

    bool connected() const override { return connected_; }
    int port_index() const override { return display_port_ - 1; }
    std::string serial() const override { return "sim-" + std::to_string(display_port_); }
    std::string system_id() const override { return system_id_for_display_port(display_port_); }
    std::string describe_device() const override { return "RocketBox sim port " + std::to_string(display_port_); }
    void connect() override;
    void disconnect() override;
    void reset_connection() override;

    void set_listen_mode(ListenMode) override {}
    void ensure_listening() override {}
    void sync_systems(std::function<void(const std::vector<SystemInfo>&)> handler) override;
    void ensure_circuit(const std::string& peer_system_id) override;
    void clear_circuit() override;
    int switch_dest() const override { return switch_dest_; }
    void send_session_message(const std::vector<uint8_t>& message) override;
    void send_announce_presence(const std::vector<uint8_t>& message, AnnouncePresenceMode) override;
    bool try_receive_session_message(unsigned, std::vector<uint8_t>*) override { return false; }

    void send_bytes(const std::vector<uint8_t>& payload, const std::string& filename) override;
    ParsedHeader receive_header() override;
    std::vector<uint8_t> receive_payload(uint64_t file_size) override;
    std::vector<uint8_t> receive_bytes() override;
    void prepare_for_payload_send() override {}
    void wait_for_idle() override {}
    void on_data_message(std::function<void(const std::vector<uint8_t>&)> cb) override;

private:
    void write_raw(const std::vector<uint8_t>& packet);
    void write_ep4_switch(int dest_port);
    void write_ep1(const std::vector<uint8_t>& data);
    void listen_loop();
    void push_inbound(std::vector<uint8_t> data);

    int display_port_;
    rb_sock_t fd_ = RB_SOCK_INVALID;
    int switch_dest_ = 0;
    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};
    std::thread thread_;
    std::mutex inbound_mu_;
    std::vector<std::vector<uint8_t>> inbound_;
    std::function<void(const std::vector<uint8_t>&)> on_msg_;
    std::vector<uint8_t> pending_header_;
    std::vector<uint8_t> pending_payload_;
};

}  // namespace detail
}  // namespace rocketbox
