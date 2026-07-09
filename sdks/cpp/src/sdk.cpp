#include "rocketbox/sdk.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace rocketbox {

namespace {

// Message Types
constexpr uint8_t MSG_ATTACH = 0x01;
constexpr uint8_t MSG_LIST = 0x02;
constexpr uint8_t MSG_CONNECT = 0x03;
constexpr uint8_t MSG_DISCONNECT = 0x04;
constexpr uint8_t MSG_KEEPALIVE = 0x05;

constexpr uint8_t MSG_ATTACHED = 0x81;
constexpr uint8_t MSG_SYSTEMS = 0x82;
constexpr uint8_t MSG_ACK = 0x83;
constexpr uint8_t MSG_NAK = 0x84;
constexpr uint8_t MSG_CIRCUIT_UP = 0x85;
constexpr uint8_t MSG_CIRCUIT_DOWN = 0x86;

struct ControlHeader {
    uint8_t ver;
    uint8_t type;
    uint16_t txn;
    uint32_t arg;
    uint32_t len;
};

// Simple big-endian conversions
uint16_t read_u16_be(const uint8_t* buf) {
    return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
}

uint32_t read_u32_be(const uint8_t* buf) {
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8)  |
           buf[3];
}

void write_u16_be(uint8_t* buf, uint16_t val) {
    buf[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(val & 0xFF);
}

void write_u32_be(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(val & 0xFF);
}

bool read_all(int fd, uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n <= 0) {
            return false;
        }
        total += n;
    }
    return true;
}

} // namespace

class ConnectionImpl : public Connection, public std::enable_shared_from_this<ConnectionImpl> {
public:
    std::string peer_id_;
    std::string state_ = "open";
    std::mutex mu_;
    std::function<void(const std::vector<uint8_t>&)> on_received_;
    std::function<void(const std::vector<uint8_t>&)> on_msg_received_;
    std::function<void(const std::string&)> on_closed_;
    std::function<void()> on_close_local_;

    ConnectionImpl(std::string peer_id, std::function<void()> on_close_local)
        : peer_id_(std::move(peer_id))
        , on_close_local_(std::move(on_close_local)) {}

    std::string GetPeerSystemId() override { return peer_id_; }
    std::string GetState() override { std::lock_guard<std::mutex> l(mu_); return state_; }

    void Send(const std::vector<uint8_t>& bytes) override {
        {
            std::lock_guard<std::mutex> l(mu_);
            if (state_ != "open") return;
        }
        // Write raw bytes to Session
        on_write_data_(bytes);
    }

    void SendMessage(const std::vector<uint8_t>& message) override {
        {
            std::lock_guard<std::mutex> l(mu_);
            if (state_ != "open") return;
        }
        // Write framed bytes to Session (4-byte BE length + data)
        std::vector<uint8_t> framed(4 + message.size());
        write_u32_be(framed.data(), static_cast<uint32_t>(message.size()));
        std::memcpy(framed.data() + 4, message.data(), message.size());
        on_write_data_(framed);
    }

    void Close() override {
        bool notify = false;
        {
            std::lock_guard<std::mutex> l(mu_);
            if (state_ == "open") {
                state_ = "closing";
                notify = true;
            }
        }
        if (notify) {
            on_close_local_();
            std::lock_guard<std::mutex> l(mu_);
            state_ = "closed";
            if (on_closed_) on_closed_("closed");
        }
    }

    void handle_data(const std::vector<uint8_t>& data) {
        std::function<void(const std::vector<uint8_t>&)> rec;
        std::function<void(const std::vector<uint8_t>&)> mrec;
        {
            std::lock_guard<std::mutex> l(mu_);
            rec = on_received_;
            mrec = on_msg_received_;
        }
        if (rec) rec(data);
        if (mrec && data.size() >= 4) {
            uint32_t msg_len = read_u32_be(data.data());
            if (msg_len == data.size() - 4) {
                std::vector<uint8_t> msg(data.begin() + 4, data.end());
                mrec(msg);
            }
        }
    }

    void handle_remote_close(const std::string& reason) {
        std::function<void(const std::string&)> closed;
        {
            std::lock_guard<std::mutex> l(mu_);
            if (state_ != "open") return;
            state_ = "closed";
            closed = on_closed_;
        }
        if (closed) closed(reason);
    }

    std::function<void(const std::vector<uint8_t>&)> on_write_data_;

    void OnReceived(std::function<void(const std::vector<uint8_t>&)> cb) override {
        std::lock_guard<std::mutex> l(mu_); on_received_ = std::move(cb);
    }
    void OnMessageReceived(std::function<void(const std::vector<uint8_t>&)> cb) override {
        std::lock_guard<std::mutex> l(mu_); on_msg_received_ = std::move(cb);
    }
    void OnClosed(std::function<void(const std::string&)> cb) override {
        std::lock_guard<std::mutex> l(mu_); on_closed_ = std::move(cb);
    }
};

class SessionImpl : public Session {
public:
    TransportType transport_;
    int port_;
    int fd_ = -1;
    std::string system_id_;
    std::atomic<bool> stop_{false};
    std::thread thread_;

    std::mutex mu_;
    std::condition_variable cv_;
    uint16_t next_txn_ = 1;

    struct TxnWait {
        bool resolved = false;
        ControlHeader header;
        std::vector<uint8_t> payload;
    };
    std::map<uint16_t, std::shared_ptr<TxnWait>> txn_waits_;

    std::shared_ptr<ConnectionImpl> active_connection_;

    std::function<void(const std::vector<SystemInfo>&)> on_systems_changed_;
    std::function<void(std::shared_ptr<Connection>)> on_incoming_circuit_;
    std::function<void()> on_detached_;

    SessionImpl(TransportType transport, int port)
        : transport_(transport), port_(port) {}

    ~SessionImpl() {
        stop_ = true;
        if (fd_ != -1) {
            shutdown(fd_, SHUT_RDWR);
            close(fd_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void init() {
        if (transport_ == TransportType::Sim) {
#ifdef ROCKETBOX_SIM_SUPPORT
            fd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (fd_ == -1) {
                throw std::runtime_error("Failed to create TCP socket");
            }
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(1772);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            if (connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                close(fd_);
                fd_ = -1;
                throw std::runtime_error("Could not connect to simulation daemon");
            }

            thread_ = std::thread([this] { listen_loop(); });

            // Send ATTACH
            send_attach();
#else
            throw std::runtime_error("Simulation support was not compiled into this SDK build.");
#endif
        } else {
            throw std::runtime_error("USB transport not implemented in MVP simulation mode");
        }
    }

    void listen_loop() {
        uint8_t ep_id = 0;
        while (!stop_) {
            if (read(fd_, &ep_id, 1) <= 0) {
                handle_detach();
                break;
            }

            if (ep_id == 0x03) {
                // EP3 Control IN
                uint8_t header_buf[12];
                if (!read_all(fd_, header_buf, 12)) {
                    handle_detach();
                    break;
                }
                ControlHeader h{};
                h.ver = header_buf[0];
                h.type = header_buf[1];
                h.txn = read_u16_be(header_buf + 2);
                h.arg = read_u32_be(header_buf + 4);
                h.len = read_u32_be(header_buf + 8);

                std::vector<uint8_t> payload(h.len);
                if (h.len > 0 && !read_all(fd_, payload.data(), h.len)) {
                    handle_detach();
                    break;
                }

                handle_control_message(h, payload);
            } else if (ep_id == 0x02) {
                // EP2 Data IN
                uint8_t len_buf[4];
                if (!read_all(fd_, len_buf, 4)) {
                    handle_detach();
                    break;
                }
                uint32_t len = read_u32_be(len_buf);
                std::vector<uint8_t> payload(len);
                if (len > 0 && !read_all(fd_, payload.data(), len)) {
                    handle_detach();
                    break;
                }

                std::shared_ptr<ConnectionImpl> conn;
                {
                    std::lock_guard<std::mutex> l(mu_);
                    conn = active_connection_;
                }
                if (conn) {
                    conn->handle_data(payload);
                }
            }
        }
    }

    void handle_control_message(const ControlHeader& h, const std::vector<uint8_t>& payload) {
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
            auto systems = parse_systems_payload(payload);
            if (on_systems_changed_) on_systems_changed_(systems);
        } else if (h.type == MSG_CIRCUIT_UP) {
            std::string peer_id(payload.begin(), payload.end());
            auto conn = std::make_shared<ConnectionImpl>(peer_id, [this] { disconnect_circuit(); });
            conn->on_write_data_ = [this](const std::vector<uint8_t>& bytes) { write_ep1(bytes); };
            active_connection_ = conn;
            if (on_incoming_circuit_) on_incoming_circuit_(conn);
        } else if (h.type == MSG_CIRCUIT_DOWN) {
            if (active_connection_) {
                active_connection_->handle_remote_close("closed");
                active_connection_.reset();
            }
        }
    }

    std::vector<SystemInfo> parse_systems_payload(const std::vector<uint8_t>& payload) {
        if (payload.size() < 4) return {};
        uint32_t count = read_u32_be(payload.data());
        std::vector<SystemInfo> systems;
        size_t offset = 4;
        for (uint32_t i = 0; i < count; i++) {
            if (offset >= payload.size()) break;
            uint8_t id_len = payload[offset++];
            std::string id(payload.begin() + offset, payload.begin() + offset + id_len);
            offset += id_len;

            uint8_t name_len = payload[offset++];
            std::string name(payload.begin() + offset, payload.begin() + offset + name_len);
            offset += name_len;

            uint8_t status_val = payload[offset++];
            std::string status = (status_val == 1) ? "reachable" : (status_val == 2) ? "busy" : "offline";

            if (id != system_id_) {
                systems.push_back({id, name, status});
            }
        }
        return systems;
    }

    void handle_detach() {
        std::lock_guard<std::mutex> l(mu_);
        if (active_connection_) {
            active_connection_->handle_remote_close("detached");
            active_connection_.reset();
        }
        if (on_detached_) on_detached_();
    }

    void send_attach() {
        auto wait = send_control_message(MSG_ATTACH, port_ - 1, {});
        system_id_ = "sys-port-" + std::to_string(wait->header.arg + 1);
    }

    std::shared_ptr<TxnWait> send_control_message(uint8_t type, uint32_t arg, const std::vector<uint8_t>& payload) {
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
        packet[0] = 0x04; // EP4 Control OUT
        packet[1] = 0x01; // ver
        packet[2] = type;
        write_u16_be(packet.data() + 3, txn);
        write_u32_be(packet.data() + 5, arg);
        write_u32_be(packet.data() + 9, static_cast<uint32_t>(payload.size()));
        if (!payload.empty()) {
            std::memcpy(packet.data() + 13, payload.data(), payload.size());
        }

        if (write(fd_, packet.data(), packet.size()) <= 0) {
            throw std::runtime_error("Disconnected from simulation daemon");
        }

        std::unique_lock<std::mutex> l(mu_);
        cv_.wait_for(l, std::chrono::seconds(5), [wait] { return wait->resolved; });

        if (!wait->resolved) {
            txn_waits_.erase(txn);
            throw std::runtime_error("Command timeout");
        }

        if (wait->header.type == MSG_NAK) {
            std::string reason = "busy";
            if (wait->header.arg == 0x02) reason = "offline";
            else if (wait->header.arg == 0x03) reason = "denied";
            else if (wait->header.arg == 0x04) reason = "invalid";
            else if (wait->header.arg == 0x05) reason = "timeout";
            throw std::runtime_error(reason);
        }

        return wait;
    }

    void write_ep1(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> packet(1 + 4 + data.size());
        packet[0] = 0x01; // EP1 Data OUT
        write_u32_be(packet.data() + 1, static_cast<uint32_t>(data.size()));
        std::memcpy(packet.data() + 5, data.data(), data.size());
        write(fd_, packet.data(), packet.size());
    }

    void disconnect_circuit() {
        std::shared_ptr<ConnectionImpl> conn;
        {
            std::lock_guard<std::mutex> l(mu_);
            conn = active_connection_;
        }
        if (!conn) return;

        // Disconnect bidirectional switching connection using real hardware Mode 11 Core spec:
        // Writes one 16-byte packet to EP4, dest_port = 0 means disconnect
        std::vector<uint8_t> packet(1 + 16, 0);
        packet[0] = 0x04; // EP4 Control OUT prefix
        packet[1] = 0 & 0x0F; // dest_port = 0

        write(fd_, packet.data(), packet.size());
        
        std::lock_guard<std::mutex> l(mu_);
        active_connection_.reset();
    }

    std::string GetSystemId() override { return system_id_; }

    std::vector<SystemInfo> ListSystems() override {
        auto wait = send_control_message(MSG_LIST, 0, {});
        return parse_systems_payload(wait->payload);
    }

    std::shared_ptr<Connection> Connect(const std::string& targetSystemId) override {
        std::shared_ptr<ConnectionImpl> active;
        {
            std::lock_guard<std::mutex> l(mu_);
            active = active_connection_;
        }
        if (active) {
            throw std::runtime_error("AlreadyConnected");
        }

        // Parse dest_port from targetSystemId (e.g., "sys-port-2" -> Port 2)
        int targetPort = 0;
        if (targetSystemId.rfind("sys-port-", 0) == 0) {
            targetPort = std::stoi(targetSystemId.substr(9));
        }

        // Connect bidirectional switching connection using real hardware Mode 11 Core spec:
        // Writes one 16-byte packet to EP4, low nibble of first byte is dest_port (1-based index)
        std::vector<uint8_t> packet(1 + 16, 0);
        packet[0] = 0x04; // EP4 Control OUT prefix
        packet[1] = targetPort & 0x0F;

        write(fd_, packet.data(), packet.size());

        auto conn = std::make_shared<ConnectionImpl>(targetSystemId, [this] { disconnect_circuit(); });
        conn->on_write_data_ = [this](const std::vector<uint8_t>& bytes) { write_ep1(bytes); };
        
        std::lock_guard<std::mutex> l(mu_);
        active_connection_ = conn;
        return conn;
    }

    void OnSystemsChanged(std::function<void(const std::vector<SystemInfo>&)> cb) override {
        std::lock_guard<std::mutex> l(mu_); on_systems_changed_ = std::move(cb);
    }
    void OnIncomingCircuit(std::function<void(std::shared_ptr<Connection>)> cb) override {
        std::lock_guard<std::mutex> l(mu_); on_incoming_circuit_ = std::move(cb);
    }
    void OnDetached(std::function<void()> cb) override {
        std::lock_guard<std::mutex> l(mu_); on_detached_ = std::move(cb);
    }
};

std::unique_ptr<Session> Attach(TransportType transport, int port) {
    auto session = std::make_unique<SessionImpl>(transport, port);
    session->init();
    return session;
}

} // namespace rocketbox
