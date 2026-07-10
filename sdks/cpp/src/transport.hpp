#pragma once

#include "protocol.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace rocketbox {
namespace detail {

class ITransport {
public:
    using ControlHandler = std::function<void(const ControlHeader&, const std::vector<uint8_t>&)>;
    using DataHandler = std::function<void(const std::vector<uint8_t>&)>;
    using DetachHandler = std::function<void()>;

    virtual ~ITransport() = default;
    virtual void start(ControlHandler on_ctrl, DataHandler on_data, DetachHandler on_detach) = 0;
    virtual void stop() = 0;
    virtual void write_ep1(const std::vector<uint8_t>& data) = 0;
    /** Packet may include leading 0x04 mux byte from Session; transport strips if needed. */
    virtual void write_ep4(const std::vector<uint8_t>& packet) = 0;
};

std::unique_ptr<ITransport> make_sim_transport();
std::unique_ptr<ITransport> make_usb_transport();

}  // namespace detail
}  // namespace rocketbox
