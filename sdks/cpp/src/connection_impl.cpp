#include "connection_impl.hpp"
#include "protocol.hpp"

#include <cstring>

namespace rocketbox {
namespace detail {

void ConnectionImpl::Send(const std::vector<uint8_t>& bytes) {
    {
        std::lock_guard<std::mutex> l(mu_);
        if (state_ != "open") return;
    }
    if (on_write_data_) on_write_data_(bytes);
}

void ConnectionImpl::SendMessage(const std::vector<uint8_t>& message) {
    {
        std::lock_guard<std::mutex> l(mu_);
        if (state_ != "open") return;
    }
    std::vector<uint8_t> framed(4 + message.size());
    write_u32_be(framed.data(), static_cast<uint32_t>(message.size()));
    if (!message.empty()) {
        std::memcpy(framed.data() + 4, message.data(), message.size());
    }
    if (on_write_data_) on_write_data_(framed);
}

void ConnectionImpl::Close() {
    bool notify = false;
    {
        std::lock_guard<std::mutex> l(mu_);
        if (state_ == "open") {
            state_ = "closing";
            notify = true;
        }
    }
    if (notify) {
        if (on_close_local_) on_close_local_();
        std::lock_guard<std::mutex> l(mu_);
        state_ = "closed";
        if (on_closed_) on_closed_("closed");
    }
}

void ConnectionImpl::handle_data(const std::vector<uint8_t>& data) {
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

void ConnectionImpl::handle_remote_close(const std::string& reason) {
    std::function<void(const std::string&)> closed;
    {
        std::lock_guard<std::mutex> l(mu_);
        if (state_ != "open") return;
        state_ = "closed";
        closed = on_closed_;
    }
    if (closed) closed(reason);
}

}  // namespace detail
}  // namespace rocketbox
