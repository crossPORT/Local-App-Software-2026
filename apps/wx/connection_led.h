#pragma once

#include "ui_colours.h"

#include <wx/dcbuffer.h>
#include <wx/panel.h>

class ConnectionLedPanel : public wxPanel {
public:
    explicit ConnectionLedPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(14, 14), wxBORDER_NONE) {
        SetMinSize(wxSize(14, 14));
        SetMaxSize(wxSize(14, 14));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(parent->GetBackgroundColour());
        Bind(wxEVT_PAINT, &ConnectionLedPanel::OnPaint, this);
    }

    void SetLedColour(const wxColour& colour) {
        if (colour_ != colour) {
            colour_ = colour;
            Refresh();
        }
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetPen(wxPen(wxColour(255, 255, 255, 38)));
        dc.SetBrush(wxBrush(colour_));
        dc.DrawRectangle(0, 0, sz.x, sz.y);
    }

    wxColour colour_{kError};
};
