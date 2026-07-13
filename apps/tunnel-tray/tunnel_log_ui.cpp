#include "tunnel_log_ui.hpp"

#include "tunnel_log.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/filefn.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

namespace tunnel_tray {
namespace {

wxString read_log_text(const std::string& path) {
  std::ifstream in(path);
  if (!in) return wxT("(empty or unreadable)");
  std::ostringstream ss;
  ss << in.rdbuf();
  const auto s = ss.str();
  if (s.empty()) return wxT("(empty)");
  return wxString(s.c_str(), wxConvUTF8);
}

}  // namespace

void show_tunnel_log(wxWindow* parent) {
  const auto path = rocketbox_tunnel_log_path();
  const wxString wpath(path.c_str(), wxConvUTF8);
  if (!wxFileExists(wpath)) rocketbox_tunnel_log("[tray] log file created");

  wxDialog dlg(parent, wxID_ANY, wxT("Tunnel log"), wxDefaultPosition, wxSize(640, 480));
  auto* root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(&dlg, wxID_ANY, wpath), 0, wxALL, 8);

  auto* text = new wxTextCtrl(&dlg, wxID_ANY, read_log_text(path), wxDefaultPosition,
                              wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxHSCROLL);
  root->Add(text, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

  auto* btns = new wxBoxSizer(wxHORIZONTAL);
  auto* refresh = new wxButton(&dlg, wxID_ANY, wxT("Refresh"));
  auto* open_ext = new wxButton(&dlg, wxID_ANY, wxT("Open externally"));
  auto* close = new wxButton(&dlg, wxID_OK, wxT("Close"));
  btns->Add(refresh, 0, wxRIGHT, 6);
  btns->Add(open_ext, 0);
  btns->AddStretchSpacer(1);
  btns->Add(close, 0);
  root->Add(btns, 0, wxEXPAND | wxALL, 8);
  dlg.SetSizer(root);

  refresh->Bind(wxEVT_BUTTON, [text, path](wxCommandEvent&) {
    text->SetValue(read_log_text(path));
    text->ShowPosition(text->GetLastPosition());
  });
  open_ext->Bind(wxEVT_BUTTON, [wpath](wxCommandEvent&) {
#if defined(__WXGTK__)
    wxExecute(wxT("xdg-open ") + wpath, wxEXEC_ASYNC);
#elif defined(__WXOSX__)
    wxExecute(wxT("open ") + wpath, wxEXEC_ASYNC);
#else
    wxLaunchDefaultApplication(wpath);
#endif
  });

  text->ShowPosition(text->GetLastPosition());
  dlg.CentreOnParent();
  dlg.ShowModal();
}

}  // namespace tunnel_tray
