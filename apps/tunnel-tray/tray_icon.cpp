#include "tray_icon.hpp"
#include "tray_settings.hpp"
#include "tray_theme.hpp"
#include "tunnel_log_ui.hpp"

namespace {
enum {
  ID_TRAY_ENABLE = wxID_HIGHEST + 1,
  ID_TRAY_DISABLE,
  ID_TRAY_LOG,
  ID_TRAY_QUIT,
  ID_TRAY_OPEN,
};
}  // namespace

TunnelTrayIcon::TunnelTrayIcon() {
  const auto settings = tunnel_tray::load_tray_settings();
  expose_ = settings.expose;
  port_ = settings.port;
  usb_ = settings.usb;
  dark_theme_ = tunnel_tray::desktop_prefers_dark();
  reload_icons();
  tick_.Bind(wxEVT_TIMER, &TunnelTrayIcon::on_tick, this);
  pulse_.Bind(wxEVT_TIMER, &TunnelTrayIcon::on_pulse, this);
  start_ayatana();
  if (!ayatana_.active()) {
    // Fallback: wx GtkStatusIcon (menu position is unreliable on Wayland).
    Bind(wxEVT_TASKBAR_LEFT_DOWN, &TunnelTrayIcon::on_left_down, this);
    Bind(wxEVT_MENU, &TunnelTrayIcon::on_menu, this);
  }
  tick_.Start(1000);
  // Prefer live helper/stats over stale settings.enabled after tray restart.
  if (proc_.running()) {
    const int live = tunnel_tray::live_tunnel_port();
    if (live > 0) port_ = live;
    persist_settings(true);
  } else if (settings.enabled) {
    set_enabled(true);
  }
  refresh_icon();
}

TunnelTrayIcon::~TunnelTrayIcon() {
  ayatana_.stop();
  if (panel_) {
    panel_->Destroy();
    panel_.release();
  }
}

void TunnelTrayIcon::start_ayatana() {
  if (!tunnel_tray::ayatana_tray_available()) return;
  tunnel_tray::AyatanaCallbacks cbs;
  cbs.open_panel = [this](unsigned user_time) { show_panel(user_time); };
  cbs.enable = [this] { CallAfter([this] { set_enabled(true); }); };
  cbs.disable = [this] { CallAfter([this] { set_enabled(false); }); };
  cbs.open_log = [this] { CallAfter([this] { open_log(); }); };
  cbs.quit = [this] { CallAfter([this] { quit_app(); }); };
  cbs.is_running = [this] { return proc_.running(); };
  if (!ayatana_.start(std::move(cbs))) return;
  write_icon_files();
}

void TunnelTrayIcon::ensure_panel() {
  if (panel_) return;
  panel_ = std::make_unique<tunnel_tray::TrayPanel>(
      nullptr,
      [this](bool enable) {
        if (panel_) {
          const auto c = panel_->controls();
          port_ = c.port;
          usb_ = c.usb;
          expose_ = c.expose;
        }
        return set_enabled(enable);
      },
      [this]() {
        if (panel_) {
          const auto c = panel_->controls();
          port_ = c.port;
          usb_ = c.usb;
          expose_ = c.expose;
        }
        return apply_expose();
      },
      [this]() { quit_app(); });
}

wxMenu* TunnelTrayIcon::CreatePopupMenu() {
  if (ayatana_.active()) return nullptr;
  return build_menu();
}

wxMenu* TunnelTrayIcon::build_menu() {
  auto* m = new wxMenu();
  m->Append(ID_TRAY_OPEN, wxT("Open control panel"));
  if (proc_.running()) m->Append(ID_TRAY_DISABLE, wxT("Disable tunnel"));
  else m->Append(ID_TRAY_ENABLE, wxT("Enable tunnel"));
  m->Append(ID_TRAY_LOG, wxT("Open log"));
  m->AppendSeparator();
  m->Append(ID_TRAY_QUIT, wxT("Quit tray"));
  return m;
}

void TunnelTrayIcon::on_left_down(wxTaskBarIconEvent&) { PopupMenu(build_menu()); }

void TunnelTrayIcon::on_menu(wxCommandEvent& ev) {
  switch (ev.GetId()) {
    case ID_TRAY_OPEN:
      show_panel();
      break;
    case ID_TRAY_ENABLE:
      set_enabled(true);
      break;
    case ID_TRAY_DISABLE:
      set_enabled(false);
      break;
    case ID_TRAY_LOG:
      open_log();
      break;
    case ID_TRAY_QUIT:
      CallAfter([this] { quit_app(); });
      break;
    default:
      break;
  }
}

void TunnelTrayIcon::open_log() { tunnel_tray::show_tunnel_log(nullptr); }

void TunnelTrayIcon::quit_app() {
  proc_.stop();
  persist_settings(false);
  ayatana_.stop();
  if (panel_) {
    panel_->Destroy();
    panel_.release();
  }
  RemoveIcon();
  wxTheApp->ExitMainLoop();
}

void TunnelTrayIcon::show_panel(unsigned user_time) {
  ensure_panel();
  const bool up = proc_.running();
  if (up) {
    const int live = tunnel_tray::live_tunnel_port();
    if (live > 0) port_ = live;
  }
  panel_->show_raise(controls_now(), up, user_time);
}
