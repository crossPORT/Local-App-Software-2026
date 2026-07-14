#pragma once

#include <wx/wx.h>

namespace settings_ui {

extern const wxColour kBg;
extern const wxColour kField;
extern const wxColour kText;
extern const wxColour kMuted;
extern const wxColour kAccent;

wxString TrimWx(const wxString& value);
wxString ProfileText(const std::string& value);
wxStaticText* MakeLabel(wxWindow* parent, const wxString& text,
                        const wxColour& fg = kText);
void StyleField(wxTextCtrl* field);
void StyleButton(wxButton* button, bool primary = false);

}  // namespace settings_ui
