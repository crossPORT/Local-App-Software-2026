#include "transport.hpp"

#include <stdexcept>

namespace rocketbox {
namespace detail {

std::unique_ptr<ITransport> make_usb_transport() {
    throw std::runtime_error("USB support was not compiled into this SDK build.");
}

}  // namespace detail
}  // namespace rocketbox
