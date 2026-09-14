#pragma once

#include <functional>
#include <string>
#include <wx/wx.h>

wxStaticText* MakeUiLabel(wxWindow* parent,
                          const wxString& text,
                          const wxColour& fg,
                          int pt = 10,
                          wxFontWeight weight = wxFONTWEIGHT_NORMAL);
void StylePrimaryButton(wxButton* button);
void StyleClearButton(wxButton* button);
std::string multi_system_meta_line(int devices_seen);
wxPanel* MakeDisconnectButton(wxWindow* parent, std::function<void()> on_click);
