#include "expose_presets.hpp"

namespace tunnel_tray {

std::vector<Endpoint> expose_presets() {
  return {
      {Proto::Tcp, 445}, {Proto::Udp, 445}, {Proto::Tcp, 22},
      {Proto::Tcp, 80},  {Proto::Tcp, 8080}, {Proto::Tcp, 443},
  };
}

}  // namespace tunnel_tray
