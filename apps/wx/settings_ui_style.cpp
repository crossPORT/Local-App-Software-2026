#include "settings_ui_style.h"

#include <algorithm>
#include <string>

namespace settings_ui {

const wxColour kBg(0x0f, 0x14, 0x19);
const wxColour kField(0x12, 0x18, 0x22);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);

wxString TrimWx(const wxString& value) {
  wxString trimmed = value;
  trimmed.Trim(true).Trim(false);
  return trimmed;
}

wxString ProfileText(const std::string& value) {
  return wxString::FromUTF8(value.c_str());
}

wxStaticText* MakeLabel(wxWindow* parent, const wxString& text, const wxColour& fg) {
  auto* label = new wxStaticText(parent, wxID_ANY, text);
  label->SetForegroundColour(fg);
  label->SetBackgroundColour(parent->GetBackgroundColour());
  return label;
}

void StyleField(wxTextCtrl* field) {
  field->SetBackgroundColour(kField);
  field->SetForegroundColour(kText);
}

void StyleButton(wxButton* button, bool primary) {
  button->SetBackgroundColour(primary ? kAccent : kField);
  button->SetForegroundColour(primary ? kBg : kText);
  const wxSize best = button->GetBestSize();
  button->SetMinSize(wxSize(std::max(best.GetWidth() + 12, 72), std::max(best.GetHeight(), 28)));
}

}  // namespace settings_ui
