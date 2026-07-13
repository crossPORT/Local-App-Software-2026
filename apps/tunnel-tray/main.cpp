#include "tray_icon.hpp"

#include <iostream>
#include <wx/snglinst.h>
#include <wx/wx.h>

class TunnelTrayApp : public wxApp {
public:
  bool OnInit() override {
    // Keep running after the control panel closes (tray icon is not a frame).
    SetExitOnFrameDelete(false);

    checker_ = new wxSingleInstanceChecker("rocketbox-tunnel-tray-" + wxGetUserId());
    if (checker_->IsAnotherRunning()) {
      wxMessageBox(wxT("RocketBox Tunnel Tray is already running.\n"
                       "Open it from the system tray (notification area);\n"
                       "use the arrow if icons are hidden."),
                   wxT("RocketBox Tunnel"), wxOK | wxICON_INFORMATION);
      delete checker_;
      checker_ = nullptr;
      return false;
    }

    if (!wxTaskBarIcon::IsAvailable()) {
      std::cerr << "[rocketbox-tunnel-tray] warning: wxTaskBarIcon::IsAvailable()"
                   "=false (set DBUS_SESSION_BUS_ADDRESS / use run-tray.sh)\n";
    }
    // Hidden top-level so some WMs still associate the process with a window.
    hidden_ = new wxFrame(nullptr, wxID_ANY, wxT("RocketBox Tunnel"));
    hidden_->Show(false);
    tray_ = new TunnelTrayIcon();
    // App menu / desktop launch should show the panel (tray-only looks like a no-op).
    CallAfter([this] {
      if (tray_) tray_->show_panel();
    });
    return true;
  }

  int OnExit() override {
    delete tray_;
    tray_ = nullptr;
    if (hidden_) {
      hidden_->Destroy();
      hidden_ = nullptr;
    }
    delete checker_;
    checker_ = nullptr;
    return wxApp::OnExit();
  }

private:
  TunnelTrayIcon* tray_ = nullptr;
  wxFrame* hidden_ = nullptr;
  wxSingleInstanceChecker* checker_ = nullptr;
};

wxIMPLEMENT_APP(TunnelTrayApp);
