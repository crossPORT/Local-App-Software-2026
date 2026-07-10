#include "transport.hpp"

#include <stdexcept>

namespace rocketbox {
namespace detail {

std::unique_ptr<ITransport> make_sim_transport() {
    throw std::runtime_error("Simulation support was not compiled into this SDK build.");
}

}  // namespace detail
}  // namespace rocketbox
