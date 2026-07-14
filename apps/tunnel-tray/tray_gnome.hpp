#pragma once

namespace tunnel_tray {

/** Call once at app start — stops GTK startup “window is ready” style notifies. */
void disable_gtk_startup_notify();

/** Strip urgency / attention so GNOME never toasts for this window. */
void suppress_window_attention(void* gtk_widget_handle);

}  // namespace tunnel_tray
