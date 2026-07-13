#include "tray_theme.hpp"

#if defined(ROCKETBOX_TRAY_HAS_GTK) && defined(__WXGTK__)
#include <gio/gio.h>
#include <gtk/gtk.h>
#endif

namespace tunnel_tray {

bool desktop_prefers_dark() {
#if defined(ROCKETBOX_TRAY_HAS_GTK) && defined(__WXGTK__)
  GSettings* iface = g_settings_new("org.gnome.desktop.interface");
  if (iface) {
    gchar* scheme = g_settings_get_string(iface, "color-scheme");
    g_object_unref(iface);
    if (scheme) {
      const bool prefer_light = g_strcmp0(scheme, "prefer-light") == 0;
      g_free(scheme);
      if (prefer_light) return false;
    }
  }
#endif
  return true;
}

}  // namespace tunnel_tray
