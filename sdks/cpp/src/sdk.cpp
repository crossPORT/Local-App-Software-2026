#include "rocketbox/sdk.h"
#include "session_impl.hpp"

namespace rocketbox {

std::unique_ptr<Session> Attach(TransportType transport, int port) {
    auto session = std::make_unique<detail::SessionImpl>(transport, port);
    session->init();
    return session;
}

}  // namespace rocketbox
