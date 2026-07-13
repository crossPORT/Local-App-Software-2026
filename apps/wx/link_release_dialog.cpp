#include "link_release_dialog.h"

#include <algorithm>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace {

const wxColour kBg(0x1a, 0x23, 0x32);
const wxColour kField(0x12, 0x18, 0x22);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kWarn(0xe8, 0xa8, 0x38);
const wxColour kAccent(0x00, 0xd4, 0xaa);

wxStaticText* MakeText(wxWindow* parent,
                       const wxString& text,
                       const wxColour& fg,
                       int pt = 11,
                       wxFontWeight weight = wxFONTWEIGHT_NORMAL) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    font.SetWeight(weight);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(kBg);
    return label;
}

void StyleButton(wxButton* button, bool primary) {
    button->SetBackgroundColour(primary ? kAccent : kField);
    button->SetForegroundColour(primary ? kBg : kText);
    const wxSize best = button->GetBestSize();
    button->SetMinSize(wxSize(std::max(best.GetWidth() + 24, 110), std::max(best.GetHeight(), 36)));
}

}  // namespace

LinkReleaseDialog::LinkReleaseDialog(wxWindow* parent,
                                     const std::string& peer_label,
                                     int display_port,
                                     bool waiting_for_accept)
    : wxDialog(parent,
               wxID_ANY,
               "Release link?",
               wxDefaultPosition,
               wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP) {
    SetBackgroundColour(kBg);
    auto* root = new wxBoxSizer(wxVERTICAL);

    wxString body = wxString::Format("Clear the switch to %s (port %d).",
                                     wxString::FromUTF8(peer_label.c_str()),
                                     display_port);
    auto* body_label = MakeText(this, body, kText, 12);
    body_label->Wrap(360);
    root->Add(body_label, 0, wxLEFT | wxRIGHT | wxTOP, 16);

    if (waiting_for_accept) {
        auto* warn = MakeText(this, "This cancels the pending offer wait.", kWarn, 11);
        warn->Wrap(360);
        root->Add(warn, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    } else {
        auto* hint =
            MakeText(this, "You can announce again anytime to rediscover peers.", kMuted, 11);
        hint->Wrap(360);
        root->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    }

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* keep = new wxButton(this, wxID_CANCEL, "Keep link");
    auto* release = new wxButton(this, wxID_OK, "Release");
    StyleButton(keep, false);
    StyleButton(release, true);
    row->Add(keep, 1, wxEXPAND | wxRIGHT, 10);
    row->Add(release, 1, wxEXPAND);
    root->Add(row, 0, wxEXPAND | wxALL, 16);

    SetSizer(root);
    root->Fit(this);
    SetMinSize(GetSize());
    CentreOnParent();
}
