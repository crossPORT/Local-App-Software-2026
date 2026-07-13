#pragma once

#include "expose_list_ui.hpp"
#include "listen_ports.hpp"

#include <functional>
#include <vector>

#include <wx/frame.h>

class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxRadioButton;
class wxStaticText;

namespace tunnel_tray {

struct TrayControls {
  int port = 1;
  bool usb = true;
  std::vector<Endpoint> expose;
};

/** Modeless control panel — Close hides; Quit tray exits via callback. */
class TrayPanel : public wxFrame {
public:
  using EnableFn = std::function<bool(bool enable)>;
  using VoidFn = std::function<void()>;

  TrayPanel(wxWindow* parent, EnableFn set_enabled, VoidFn on_expose, VoidFn on_quit);
  void show_raise(const TrayControls& ctrls, bool running);
  void hide_panel();
  bool is_shown() const { return IsShown(); }
  void sync_from_host(const TrayControls& ctrls, bool running);
  TrayControls controls() const { return ctrls_; }

private:
  void build_ui();
  void refill_ports();
  void sync_ctrls();
  void read_expose();
  void on_enable(wxCommandEvent&);
  void on_close(wxCloseEvent&);

  EnableFn set_enabled_;
  VoidFn on_expose_;
  VoidFn on_quit_;
  TrayControls ctrls_;
  bool running_ = false;

  wxStaticText* status_ = nullptr;
  wxCheckBox* enable_ = nullptr;
  wxStaticText* cable_lbl_ = nullptr;
  wxChoice* port_ = nullptr;
  wxRadioButton* usb_ = nullptr;
  wxRadioButton* sim_ = nullptr;
  wxCheckListBox* list_ = nullptr;
  std::vector<ExposeRow> rows_;
  std::vector<int> port_ids_;
};

}  // namespace tunnel_tray
