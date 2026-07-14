#pragma once

#include <functional>
#include <string>

namespace tunnel_tray {

struct AyatanaCallbacks {
  std::function<void(unsigned user_time)> open_panel;
  std::function<void()> enable;
  std::function<void()> disable;
  std::function<void()> open_log;
  std::function<void()> about;
  std::function<void()> quit;
  std::function<bool()> is_running;
};

/** GNOME/KDE tray via Ayatana — shell places the menu under the icon. */
class AyatanaTray {
public:
  AyatanaTray() = default;
  ~AyatanaTray() { stop(); }
  AyatanaTray(const AyatanaTray&) = delete;
  AyatanaTray& operator=(const AyatanaTray&) = delete;

  bool start(AyatanaCallbacks cbs);
  void stop();
  bool active() const { return ind_ != nullptr; }
  void set_icon(const std::string& path, const std::string& tip);
  void refresh_menu();

private:
  void* ind_ = nullptr;  // AppIndicator*
  void* menu_ = nullptr; // GtkWidget* menu
  AyatanaCallbacks cbs_;
  std::string icon_path_;
};

bool ayatana_tray_available();

}  // namespace tunnel_tray
