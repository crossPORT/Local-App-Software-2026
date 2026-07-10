#include "session_impl.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace rocketbox {
namespace detail {

SessionImpl::SessionImpl(TransportType transport, int port)
    : transport_type_(transport), port_(port) {}

SessionImpl::~SessionImpl() {
    if (transport_) transport_->stop();
}

void SessionImpl::init() {
    if (transport_type_ == TransportType::Sim) {
#ifdef ROCKETBOX_SIM_SUPPORT
        transport_ = make_sim_transport();
#else
        throw std::runtime_error("Simulation support was not compiled into this SDK build.");
#endif
    } else {
        transport_ = make_usb_transport();
    }

    transport_->start(
        [this](const ControlHeader& h, const std::vector<uint8_t>& p) { handle_control(h, p); },
        [this](const std::vector<uint8_t>& p) { handle_data(p); },
        [this] { handle_detach(); });
    send_attach();
}

void SessionImpl::handle_data(const std::vector<uint8_t>& payload) {
    std::shared_ptr<ConnectionImpl> conn;
    {
        std::lock_guard<std::mutex> l(mu_);
        conn = active_connection_;
    }
    if (conn) conn->handle_data(payload);
}

void SessionImpl::handle_detach() {
    std::lock_guard<std::mutex> l(mu_);
    if (active_connection_) {
        active_connection_->handle_remote_close("detached");
        active_connection_.reset();
    }
    if (on_detached_) on_detached_();
}

void SessionImpl::handle_control(const ControlHeader& h, const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> l(mu_);
    if (txn_waits_.count(h.txn)) {
        auto wait = txn_waits_[h.txn];
        wait->resolved = true;
        wait->header = h;
        wait->payload = payload;
        cv_.notify_all();
        return;
    }
    if (h.type == MSG_SYSTEMS) {
        auto systems = parse_systems(payload);
        if (on_systems_changed_) on_systems_changed_(systems);
    } else if (h.type == MSG_CIRCUIT_UP) {
        std::string peer_id(payload.begin(), payload.end());
        auto conn = std::make_shared<ConnectionImpl>(peer_id, [this] { disconnect_circuit(); });
        conn->on_write_data_ = [this](const std::vector<uint8_t>& bytes) {
            transport_->write_ep1(bytes);
        };
        active_connection_ = conn;
        if (on_incoming_circuit_) on_incoming_circuit_(conn);
    } else if (h.type == MSG_CIRCUIT_DOWN) {
        if (active_connection_) {
            active_connection_->handle_remote_close("closed");
            active_connection_.reset();
        }
    }
}

void SessionImpl::send_attach() {
    auto wait = send_control(MSG_ATTACH, static_cast<uint32_t>(port_ - 1), {});
    system_id_ = "sys-port-" + std::to_string(wait->header.arg + 1);
}

std::shared_ptr<SessionImpl::TxnWait> SessionImpl::send_control(
    uint8_t type, uint32_t arg, const std::vector<uint8_t>& payload) {
    uint16_t txn = 0;
    std::shared_ptr<TxnWait> wait;
    {
        std::lock_guard<std::mutex> l(mu_);
        txn = next_txn_++;
        if (next_txn_ == 0) next_txn_ = 1;
        wait = std::make_shared<TxnWait>();
        txn_waits_[txn] = wait;
    }

    std::vector<uint8_t> packet(1 + 12 + payload.size());
    packet[0] = 0x04;
    packet[1] = 0x01;
    packet[2] = type;
    write_u16_be(packet.data() + 3, txn);
    write_u32_be(packet.data() + 5, arg);
    write_u32_be(packet.data() + 9, static_cast<uint32_t>(payload.size()));
    if (!payload.empty()) {
        std::memcpy(packet.data() + 13, payload.data(), payload.size());
    }
    transport_->write_ep4(packet);

    std::unique_lock<std::mutex> l(mu_);
    cv_.wait_for(l, std::chrono::seconds(5), [wait] { return wait->resolved; });
    if (!wait->resolved) {
        txn_waits_.erase(txn);
        throw std::runtime_error("Command timeout");
    }
    if (wait->header.type == MSG_NAK) {
        throw std::runtime_error(nak_reason(wait->header.arg));
    }
    return wait;
}

void SessionImpl::disconnect_circuit() {
    std::shared_ptr<ConnectionImpl> conn;
    {
        std::lock_guard<std::mutex> l(mu_);
        conn = active_connection_;
    }
    if (!conn) return;
    send_control(MSG_DISCONNECT, 0, {});
    std::lock_guard<std::mutex> l(mu_);
    active_connection_.reset();
}

std::vector<SystemInfo> SessionImpl::ListSystems() {
    return parse_systems(send_control(MSG_LIST, 0, {})->payload);
}

std::shared_ptr<Connection> SessionImpl::Connect(const std::string& targetSystemId) {
    {
        std::lock_guard<std::mutex> l(mu_);
        if (active_connection_) throw std::runtime_error("AlreadyConnected");
    }
    std::vector<uint8_t> payload(targetSystemId.begin(), targetSystemId.end());
    send_control(MSG_CONNECT, 0, payload);

    auto conn = std::make_shared<ConnectionImpl>(targetSystemId, [this] { disconnect_circuit(); });
    conn->on_write_data_ = [this](const std::vector<uint8_t>& bytes) {
        transport_->write_ep1(bytes);
    };
    std::lock_guard<std::mutex> l(mu_);
    active_connection_ = conn;
    return conn;
}

std::vector<SystemInfo> SessionImpl::parse_systems(const std::vector<uint8_t>& payload) {
    if (payload.size() < 4) return {};
    uint32_t count = read_u32_be(payload.data());
    std::vector<SystemInfo> systems;
    size_t offset = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (offset >= payload.size()) break;
        uint8_t id_len = payload[offset++];
        std::string id(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                       payload.begin() + static_cast<std::ptrdiff_t>(offset + id_len));
        offset += id_len;
        uint8_t name_len = payload[offset++];
        std::string name(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                         payload.begin() + static_cast<std::ptrdiff_t>(offset + name_len));
        offset += name_len;
        uint8_t status_val = payload[offset++];
        std::string status =
            (status_val == 1) ? "reachable" : (status_val == 2) ? "busy" : "offline";
        if (id != system_id_) systems.push_back({id, name, status});
    }
    return systems;
}

void SessionImpl::OnSystemsChanged(std::function<void(const std::vector<SystemInfo>&)> cb) {
    std::lock_guard<std::mutex> l(mu_);
    on_systems_changed_ = std::move(cb);
}
void SessionImpl::OnIncomingCircuit(std::function<void(std::shared_ptr<Connection>)> cb) {
    std::lock_guard<std::mutex> l(mu_);
    on_incoming_circuit_ = std::move(cb);
}
void SessionImpl::OnDetached(std::function<void()> cb) {
    std::lock_guard<std::mutex> l(mu_);
    on_detached_ = std::move(cb);
}

}  // namespace detail
}  // namespace rocketbox
