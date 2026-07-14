#include "tray_ayatana.hpp"

#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
#include <libayatana-appindicator/app-indicator.h>
#endif

namespace tunnel_tray {
namespace {

#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
enum Act { kOpen = 1, kEnable, kDisable, kLog, kQuit };

void on_activate(GtkMenuItem*, gpointer data) {
  const auto act = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(data), "rb-act"));
  auto* cbs = static_cast<AyatanaCallbacks*>(g_object_get_data(G_OBJECT(data), "rb-cbs"));
  if (!cbs) return;
  switch (act) {
    case kOpen:
      if (cbs->open_panel) {
        cbs->open_panel(static_cast<unsigned>(gtk_get_current_event_time()));
      }
      break;
    case kEnable:
      if (cbs->enable) cbs->enable();
      break;
    case kDisable:
      if (cbs->disable) cbs->disable();
      break;
    case kLog:
      if (cbs->open_log) cbs->open_log();
      break;
    case kQuit:
      if (cbs->quit) cbs->quit();
      break;
    default:
      break;
  }
}

GtkWidget* add_item(GtkWidget* menu, const char* label, int act, AyatanaCallbacks* cbs) {
  GtkWidget* item = gtk_menu_item_new_with_label(label);
  g_object_set_data(G_OBJECT(item), "rb-act", GINT_TO_POINTER(act));
  g_object_set_data(G_OBJECT(item), "rb-cbs", cbs);
  g_signal_connect(item, "activate", G_CALLBACK(on_activate), item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  return item;
}
#endif

}  // namespace

bool ayatana_tray_available() {
#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
  return true;
#else
  return false;
#endif
}

bool AyatanaTray::start(AyatanaCallbacks cbs) {
#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
  stop();
  cbs_ = std::move(cbs);
  ind_ = app_indicator_new("rocketbox-tunnel", "rocketbox",
                           APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
  if (!ind_) return false;
  app_indicator_set_status(APP_INDICATOR(ind_), APP_INDICATOR_STATUS_ACTIVE);
  refresh_menu();
  return true;
#else
  (void)cbs;
  return false;
#endif
}

void AyatanaTray::stop() {
#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
  if (ind_) {
    app_indicator_set_status(APP_INDICATOR(ind_), APP_INDICATOR_STATUS_PASSIVE);
    g_object_unref(ind_);
    ind_ = nullptr;
  }
  menu_ = nullptr;
#endif
}

void AyatanaTray::set_icon(const std::string& path, const std::string& tip) {
#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
  if (!ind_ || path.empty()) return;
  icon_path_ = path;
  app_indicator_set_icon_full(APP_INDICATOR(ind_), path.c_str(),
                              tip.empty() ? "RocketBox Tunnel" : tip.c_str());
  if (!tip.empty()) app_indicator_set_title(APP_INDICATOR(ind_), tip.c_str());
#else
  (void)path;
  (void)tip;
#endif
}

void AyatanaTray::refresh_menu() {
#if defined(ROCKETBOX_TRAY_HAS_AYATANA)
  if (!ind_) return;
  GtkWidget* menu = gtk_menu_new();
  add_item(menu, "Open control panel", kOpen, &cbs_);
  const bool up = cbs_.is_running && cbs_.is_running();
  if (up) add_item(menu, "Disable tunnel", kDisable, &cbs_);
  else add_item(menu, "Enable tunnel", kEnable, &cbs_);
  add_item(menu, "Open log", kLog, &cbs_);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
  add_item(menu, "Quit tray", kQuit, &cbs_);
  gtk_widget_show_all(menu);
  app_indicator_set_menu(APP_INDICATOR(ind_), GTK_MENU(menu));
  menu_ = menu;
#endif
}

}  // namespace tunnel_tray
