#include "transport.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace rocketbox {
namespace detail {
namespace {

bool read_all(int fd, uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::read(fd, buf + total, len - total);
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

}  // namespace

class SimTransport final : public ITransport {
public:
    ~SimTransport() override { stop(); }

    void start(ControlHandler on_ctrl, DataHandler on_data, DetachHandler on_detach) override {
        on_ctrl_ = std::move(on_ctrl);
        on_data_ = std::move(on_data);
        on_detach_ = std::move(on_detach);

        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) throw std::runtime_error("Failed to create TCP socket");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1772);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("Could not connect to simulation daemon");
        }

        stop_ = false;
        thread_ = std::thread([this] { listen_loop(); });
    }

    void stop() override {
        stop_ = true;
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
        if (thread_.joinable()) thread_.join();
    }

    void write_ep1(const std::vector<uint8_t>& data) override {
        std::vector<uint8_t> packet(1 + 4 + data.size());
        packet[0] = 0x01;
        write_u32_be(packet.data() + 1, static_cast<uint32_t>(data.size()));
        if (!data.empty()) {
            std::memcpy(packet.data() + 5, data.data(), data.size());
        }
        write_raw(packet);
    }

    void write_ep4(const std::vector<uint8_t>& packet) override { write_raw(packet); }

private:
    void write_raw(const std::vector<uint8_t>& packet) {
        if (fd_ < 0) throw std::runtime_error("Disconnected from simulation daemon");
        if (::write(fd_, packet.data(), packet.size()) <= 0) {
            throw std::runtime_error("Disconnected from simulation daemon");
        }
    }

    void listen_loop() {
        uint8_t ep_id = 0;
        while (!stop_) {
            if (::read(fd_, &ep_id, 1) <= 0) {
                if (on_detach_) on_detach_();
                break;
            }
            if (ep_id == 0x03) {
                uint8_t hdr[12];
                if (!read_all(fd_, hdr, 12)) {
                    if (on_detach_) on_detach_();
                    break;
                }
                auto h = parse_control_header(hdr);
                std::vector<uint8_t> payload(h.len);
                if (h.len > 0 && !read_all(fd_, payload.data(), h.len)) {
                    if (on_detach_) on_detach_();
                    break;
                }
                if (on_ctrl_) on_ctrl_(h, payload);
            } else if (ep_id == 0x02) {
                uint8_t len_buf[4];
                if (!read_all(fd_, len_buf, 4)) {
                    if (on_detach_) on_detach_();
                    break;
                }
                uint32_t len = read_u32_be(len_buf);
                std::vector<uint8_t> payload(len);
                if (len > 0 && !read_all(fd_, payload.data(), len)) {
                    if (on_detach_) on_detach_();
                    break;
                }
                if (on_data_) on_data_(payload);
            }
        }
    }

    int fd_ = -1;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    ControlHandler on_ctrl_;
    DataHandler on_data_;
    DetachHandler on_detach_;
};

std::unique_ptr<ITransport> make_sim_transport() {
    return std::make_unique<SimTransport>();
}

}  // namespace detail
}  // namespace rocketbox
