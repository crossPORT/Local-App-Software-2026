#include "tray_theme.hpp"

#if defined(ROCKETBOX_TRAY_HAS_GTK) && defined(__WXGTK__)
#include <gio/gio.h>
#include <gtk/gtk.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tunnel_tray {

bool desktop_prefers_dark() {
#if defined(ROCKETBOX_TRAY_HAS_GTK) && defined(__WXGTK__)
  GSettings* iface = g_settings_new("org.gnome.desktop.interface");
  if (iface) {
    gchar* scheme = g_settings_get_string(iface, "color-scheme");
    g_object_unref(iface);
    if (scheme) {
      const bool dark = g_strcmp0(scheme, "prefer-dark") == 0;
      g_free(scheme);
      return dark;
    }
  }
#elif defined(_WIN32)
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                    KEY_READ, &key) == ERROR_SUCCESS) {
    DWORD val = 1;
    DWORD n = sizeof(val);
    const LONG rc =
        RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&val), &n);
    RegCloseKey(key);
    if (rc == ERROR_SUCCESS) return val == 0;
  }
#endif
  return false;
}

}  // namespace tunnel_tray
