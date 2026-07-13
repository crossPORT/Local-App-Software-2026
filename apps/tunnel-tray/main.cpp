#include "tray_icon.hpp"
#include "tray_ipc.hpp"
#include "session_dbus.hpp"

#include <iostream>
#include <wx/cmdline.h>
#include <wx/snglinst.h>
#include <wx/wx.h>

class TunnelTrayApp : public wxApp {
public:
  bool Initialize(int& argc, wxChar** argv) override {
    // Before GTK init — required for StatusNotifier / tray icon on GNOME.
    adopt_session_dbus_env();
    return wxApp::Initialize(argc, argv);
  }

  bool OnInit() override {
    SetExitOnFrameDelete(false);
    if (!wxApp::OnInit()) return false;

    checker_ = new wxSingleInstanceChecker("rocketbox-tunnel-tray-" + wxGetUserId());
    if (checker_->IsAnotherRunning()) {
      if (!tunnel_tray::activate_existing_tray()) {
        std::cerr << "[rocketbox-tunnel-tray] already running (open from system tray)\n";
      }
      delete checker_;
      checker_ = nullptr;
      return false;
    }

    if (!wxTaskBarIcon::IsAvailable()) {
      std::cerr << "[rocketbox-tunnel-tray] warning: tray unavailable "
                   "(launch via apps/tunnel-tray/run-tray.sh so DBUS_SESSION_BUS_ADDRESS is set)\n";
    }

    // Anchor frame — never Show(); size must be >0 to avoid gtk_window_resize asserts.
    hidden_ = new wxFrame(nullptr, wxID_ANY, wxT("RocketBox Tunnel"), wxDefaultPosition,
                          wxSize(200, 100), wxFRAME_NO_TASKBAR | wxFRAME_TOOL_WINDOW);
    tray_ = new TunnelTrayIcon();
    ipc_ = new tunnel_tray::TrayIpcServer([this] {
      if (tray_) tray_->show_panel();
    });
    if (!background_) {
      CallAfter([this] {
        if (tray_) tray_->show_panel();
      });
    }
    return true;
  }

  void OnInitCmdLine(wxCmdLineParser& parser) override {
    wxApp::OnInitCmdLine(parser);
    parser.AddSwitch("bg", "background", "start in tray without opening the panel");
  }

  bool OnCmdLineParsed(wxCmdLineParser& parser) override {
    background_ = parser.Found("background") || parser.Found("bg");
    return wxApp::OnCmdLineParsed(parser);
  }

  int OnExit() override {
    delete ipc_;
    ipc_ = nullptr;
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
  bool background_ = false;
  TunnelTrayIcon* tray_ = nullptr;
  wxFrame* hidden_ = nullptr;
  wxSingleInstanceChecker* checker_ = nullptr;
  tunnel_tray::TrayIpcServer* ipc_ = nullptr;
};

wxIMPLEMENT_APP(TunnelTrayApp);
