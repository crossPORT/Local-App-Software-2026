#pragma once

#include "rocketbox/sdk.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace rocketbox {
namespace detail {

class ConnectionImpl : public Connection, public std::enable_shared_from_this<ConnectionImpl> {
public:
    std::string peer_id_;
    std::string state_ = "open";
    std::mutex mu_;
    std::function<void(const std::vector<uint8_t>&)> on_received_;
    std::function<void(const std::vector<uint8_t>&)> on_msg_received_;
    std::function<void(const std::string&)> on_closed_;
    std::function<void()> on_close_local_;
    std::function<void(const std::vector<uint8_t>&)> on_write_data_;

    ConnectionImpl(std::string peer_id, std::function<void()> on_close_local)
        : peer_id_(std::move(peer_id)), on_close_local_(std::move(on_close_local)) {}

    std::string GetPeerSystemId() override { return peer_id_; }
    std::string GetState() override {
        std::lock_guard<std::mutex> l(mu_);
        return state_;
    }

    void Send(const std::vector<uint8_t>& bytes) override;
    void SendMessage(const std::vector<uint8_t>& message) override;
    void Close() override;
    void handle_data(const std::vector<uint8_t>& data);
    void handle_remote_close(const std::string& reason);

    void OnReceived(std::function<void(const std::vector<uint8_t>&)> cb) override {
        std::lock_guard<std::mutex> l(mu_);
        on_received_ = std::move(cb);
    }
    void OnMessageReceived(std::function<void(const std::vector<uint8_t>&)> cb) override {
        std::lock_guard<std::mutex> l(mu_);
        on_msg_received_ = std::move(cb);
    }
    void OnClosed(std::function<void(const std::string&)> cb) override {
        std::lock_guard<std::mutex> l(mu_);
        on_closed_ = std::move(cb);
    }
};

}  // namespace detail
}  // namespace rocketbox
