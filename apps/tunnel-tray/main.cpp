#include "tray_icon.hpp"

#include <iostream>
#include <wx/wx.h>

class TunnelTrayApp : public wxApp {
public:
  bool OnInit() override {
    // Keep running after the control panel closes (tray icon is not a frame).
    SetExitOnFrameDelete(false);
    if (!wxTaskBarIcon::IsAvailable()) {
      std::cerr << "[rocketbox-tunnel-tray] warning: wxTaskBarIcon::IsAvailable()"
                   "=false (set DBUS_SESSION_BUS_ADDRESS / use run-tray.sh)\n";
    }
    // Hidden top-level so some WMs still associate the process with a window.
    hidden_ = new wxFrame(nullptr, wxID_ANY, wxT("RocketBox Tunnel"));
    hidden_->Show(false);
    tray_ = new TunnelTrayIcon();
    return true;
  }

  int OnExit() override {
    delete tray_;
    tray_ = nullptr;
    if (hidden_) {
      hidden_->Destroy();
      hidden_ = nullptr;
    }
    return wxApp::OnExit();
  }

private:
  TunnelTrayIcon* tray_ = nullptr;
  wxFrame* hidden_ = nullptr;
};

wxIMPLEMENT_APP(TunnelTrayApp);
