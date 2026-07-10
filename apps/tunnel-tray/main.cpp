#include "tray_icon.hpp"

#include <wx/wx.h>

class TunnelTrayApp : public wxApp {
public:
  bool OnInit() override {
    if (!wxTaskBarIcon::IsAvailable()) {
      wxMessageBox("System tray is not available on this desktop.", "RocketBox Tunnel",
                   wxOK | wxICON_ERROR);
      return false;
    }
    tray_ = new TunnelTrayIcon();
    return true;
  }

  int OnExit() override {
    delete tray_;
    tray_ = nullptr;
    return wxApp::OnExit();
  }

private:
  TunnelTrayIcon* tray_ = nullptr;
};

wxIMPLEMENT_APP(TunnelTrayApp);
