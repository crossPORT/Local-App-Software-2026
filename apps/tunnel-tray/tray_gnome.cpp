#include "tray_gnome.hpp"

#if defined(__WXGTK__) || defined(ROCKETBOX_TRAY_HAS_GTK)
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#ifndef GDK_WINDOW_STATE_DEMANDS_ATTENTION
#define GDK_WINDOW_STATE_DEMANDS_ATTENTION static_cast<GdkWindowState>(1 << 9)
#endif
#endif

namespace tunnel_tray {
namespace {

#if defined(__WXGTK__) || defined(ROCKETBOX_TRAY_HAS_GTK)
gboolean on_window_state(GtkWidget* widget, GdkEventWindowState* event, gpointer) {
  if (!(event->changed_mask & GDK_WINDOW_STATE_DEMANDS_ATTENTION)) return FALSE;
  if (!(event->new_window_state & GDK_WINDOW_STATE_DEMANDS_ATTENTION)) return FALSE;
  // Immediately drop attention so GNOME never shows “is ready”.
  gtk_window_set_urgency_hint(GTK_WINDOW(widget), FALSE);
  if (GdkWindow* gw = gtk_widget_get_window(widget)) {
    gdk_window_set_urgency_hint(gw, FALSE);
  }
  return FALSE;
}
#endif

}  // namespace

void disable_gtk_startup_notify() {
#if defined(__WXGTK__) || defined(ROCKETBOX_TRAY_HAS_GTK)
  gtk_window_set_auto_startup_notification(FALSE);
#endif
}

void suppress_window_attention(void* gtk_widget_handle) {
#if defined(__WXGTK__) || defined(ROCKETBOX_TRAY_HAS_GTK)
  if (!gtk_widget_handle) return;
  auto* w = static_cast<GtkWidget*>(gtk_widget_handle);
  if (!GTK_IS_WINDOW(w)) return;
  gtk_window_set_urgency_hint(GTK_WINDOW(w), FALSE);
  gtk_window_set_focus_on_map(GTK_WINDOW(w), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(w), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(w), TRUE);
  if (GdkWindow* gw = gtk_widget_get_window(w)) {
    gdk_window_set_urgency_hint(gw, FALSE);
  }
  if (!g_object_get_data(G_OBJECT(w), "rb-attn-kill")) {
    g_signal_connect(w, "window-state-event", G_CALLBACK(on_window_state), nullptr);
    g_object_set_data(G_OBJECT(w), "rb-attn-kill", GINT_TO_POINTER(1));
  }
#else
  (void)gtk_widget_handle;
#endif
}

}  // namespace tunnel_tray
