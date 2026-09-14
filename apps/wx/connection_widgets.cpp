#include "connection_widgets.h"

#include "ui_colours.h"

#include <wx/dcbuffer.h>
#include <wx/sizer.h>

wxStaticText* MakeUiLabel(wxWindow* parent,
                          const wxString& text,
                          const wxColour& fg,
                          int pt,
                          wxFontWeight weight) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    font.SetWeight(weight);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(parent->GetBackgroundColour());
    return label;
}

void StylePrimaryButton(wxButton* button) {
    button->SetBackgroundColour(kButton);
    button->SetForegroundColour(kOnButton);
    const wxSize best = button->GetBestSize();
    button->SetMinSize(wxSize(-1, std::max(best.GetHeight() + 12, 52)));
    wxFont font = button->GetFont();
    font.SetPointSize(font.GetPointSize() + 2);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    button->SetFont(font);
}

void StyleClearButton(wxButton* button) {
    button->SetBackgroundColour(kAppBg);
    button->SetForegroundColour(kText);
    button->SetMinSize(wxSize(72, 52));
}

std::string multi_system_meta_line(int devices_seen) {
    if (devices_seen <= 1) {
        return {};
    }
    const int others = devices_seen - 1;
    if (others == 1) {
        return "1 other system on this fabric";
    }
    return std::to_string(others) + " other systems on this fabric";
}

namespace {

class DisconnectButton : public wxPanel {
public:
    explicit DisconnectButton(wxWindow* parent, std::function<void()> on_click)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
        , on_click_(std::move(on_click)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(kSurface);
        SetMinSize(wxSize(-1, 34));
        SetCursor(wxCursor(wxCURSOR_HAND));
        auto* sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* label = MakeUiLabel(this, "Disconnect", kText, 10);
        sizer->AddStretchSpacer();
        sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL);
        sizer->AddStretchSpacer();
        SetSizer(sizer);
        Bind(wxEVT_PAINT, &DisconnectButton::OnPaint, this);
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
            event.StopPropagation();
            if (on_click_) {
                on_click_();
            }
        });
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) {
            return;
        }
        dc.SetPen(wxPen(kBorder));
        dc.SetBrush(wxBrush(kAppBg));
        dc.DrawRoundedRectangle(0, 0, sz.x - 1, sz.y - 1, 8);
    }

    std::function<void()> on_click_;
};

}  // namespace

wxPanel* MakeDisconnectButton(wxWindow* parent, std::function<void()> on_click) {
    return new DisconnectButton(parent, std::move(on_click));
}
