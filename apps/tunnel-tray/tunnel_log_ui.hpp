#pragma once

class wxWindow;

namespace tunnel_tray {

/** Show tunnel.log in a read-only dialog (works without a MIME handler). */
void show_tunnel_log(wxWindow* parent);

}  // namespace tunnel_tray
