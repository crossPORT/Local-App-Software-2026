#include "transfer_progress_panel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {

const wxColour kPanel(0x1a, 0x23, 0x32);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kSuccess(0x3d, 0xdb, 0x8a);
const wxColour kError(0xff, 0x6b, 0x6b);
const wxColour kWarn(0xff, 0x9f, 0x43);
const wxColour kTrack(0x24, 0x30, 0x42);

constexpr int kBarHeight = 18;

enum class Phase { Idle, Waiting, Transferring, Complete, Failed };

wxStaticText* MakeLabel(wxWindow* parent,
                        const wxString& text,
                        const wxColour& fg,
                        int pt = 10,
                        wxFontWeight weight = wxFONTWEIGHT_NORMAL) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    font.SetWeight(weight);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(parent->GetBackgroundColour());
    return label;
}

std::string format_mib(uint64_t bytes) {
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream out;
    if (mib >= 100.0) {
        out << std::fixed << std::setprecision(0) << mib;
    } else if (mib >= 10.0) {
        out << std::fixed << std::setprecision(1) << mib;
    } else {
        out << std::fixed << std::setprecision(2) << mib;
    }
    out << " MiB";
    return out.str();
}

bool looks_complete(const std::string& status, const std::string& notification) {
    auto has = [](const std::string& hay, const char* needle) {
        return hay.find(needle) != std::string::npos;
    };
    return has(notification, "Received") || has(notification, "received")
           || has(status, "complete") || has(status, "Complete")
           || has(status, "finished");
}

Phase derive_phase(bool busy,
                   bool waiting,
                   uint64_t bytes_total,
                   const std::string& status,
                   const std::string& notification,
                   const std::string& error) {
    if (!error.empty() && !busy) {
        return Phase::Failed;
    }
    if (busy && waiting) {
        return Phase::Waiting;
    }
    if (busy && bytes_total > 0) {
        return Phase::Transferring;
    }
    if (!busy && looks_complete(status, notification)) {
        return Phase::Complete;
    }
    return Phase::Idle;
}

wxString phase_title(Phase phase, bool fabric_connected) {
    switch (phase) {
        case Phase::Idle:
            return fabric_connected ? "Ready to transfer" : "Fabric offline";
        case Phase::Waiting:
            return "Connecting…";
        case Phase::Transferring:
            return "Transferring";
        case Phase::Complete:
            return "Transfer complete";
        case Phase::Failed:
            return "Transfer failed";
    }
    return "Ready";
}

wxColour phase_colour(Phase phase) {
    switch (phase) {
        case Phase::Idle:
            return kMuted;
        case Phase::Waiting:
            return kWarn;
        case Phase::Transferring:
            return kAccent;
        case Phase::Complete:
            return kSuccess;
        case Phase::Failed:
            return kError;
    }
    return kMuted;
}

}  // namespace

// Custom themed progress bar: rounded track + fill in the app palette. Replaces
// wxGauge because the GTK gauge renders thin and in the system accent (orange),
// which clashes with the RocketBox theme and can't be reliably recoloured.
class ThemeProgressBar : public wxPanel {
public:
    explicit ThemeProgressBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, kBarHeight)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(-1, kBarHeight));
        SetBackgroundColour(kPanel);
        Bind(wxEVT_PAINT, &ThemeProgressBar::OnPaint, this);
    }

    void SetFraction(double fraction) {
        indeterminate_ = false;
        fraction_ = std::clamp(fraction, 0.0, 1.0);
        Refresh();
    }

    void Pulse() {
        indeterminate_ = true;
        pulse_pos_ += 0.06;
        if (pulse_pos_ > 1.0) {
            pulse_pos_ = 0.0;
        }
        Refresh();
    }

    void SetActiveColour(const wxColour& colour) {
        if (active_ != colour) {
            active_ = colour;
            Refresh();
        }
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) {
            return;
        }
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(kPanel));
        dc.DrawRectangle(0, 0, sz.x, sz.y);

        const int radius = sz.y / 2;
        dc.SetBrush(wxBrush(kTrack));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, radius);

        dc.SetBrush(wxBrush(active_));
        if (indeterminate_) {
            const int seg = std::max(sz.x / 3, sz.y);
            int x = static_cast<int>(pulse_pos_ * (sz.x + seg)) - seg;
            x = std::clamp(x, 0, std::max(0, sz.x - seg));
            dc.DrawRoundedRectangle(x, 0, seg, sz.y, radius);
        } else if (fraction_ > 0.0) {
            const int w = std::max(static_cast<int>(fraction_ * sz.x), sz.y);
            dc.DrawRoundedRectangle(0, 0, std::min(w, sz.x), sz.y, radius);
        }
    }

    double fraction_ = 0.0;
    bool indeterminate_ = false;
    double pulse_pos_ = 0.0;
    wxColour active_ = kAccent;
};

TransferProgressPanel::TransferProgressPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(kPanel);
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(MakeLabel(this, "TRANSFER", kAccent, 9, wxFONTWEIGHT_BOLD),
              0, wxLEFT | wxRIGHT | wxTOP, 12);

    phase_label_ = MakeLabel(this, "Ready to transfer", kMuted, 13, wxFONTWEIGHT_BOLD);
    root->Add(phase_label_, 0, wxLEFT | wxRIGHT | wxTOP, 8);

    auto* bar_row = new wxBoxSizer(wxHORIZONTAL);
    bar_ = new ThemeProgressBar(this);
    bar_row->Add(bar_, 1, wxALIGN_CENTER_VERTICAL);
    percent_label_ = MakeLabel(this, "", kText, 11, wxFONTWEIGHT_BOLD);
    percent_label_->SetMinSize(wxSize(48, -1));
    bar_row->Add(percent_label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    root->Add(bar_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

    auto* info_row = new wxBoxSizer(wxHORIZONTAL);
    bytes_label_ = MakeLabel(this, "", kMuted, 9);
    info_row->Add(bytes_label_, 0, wxALIGN_CENTER_VERTICAL);
    info_row->AddStretchSpacer();
    speed_label_ = MakeLabel(this, "", kAccent, 10, wxFONTWEIGHT_BOLD);
    info_row->Add(speed_label_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(info_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    message_label_ = MakeLabel(this, "Drop onto a peer to send, or press Send…",
                               kMuted, 9);
    message_label_->Wrap(440);
    root->Add(message_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 12);

    SetSizer(root);
    bar_->Hide();
    percent_label_->Hide();
    bytes_label_->Hide();
    speed_label_->Hide();
}

void TransferProgressPanel::ApplyState(bool busy,
                                       bool waiting,
                                       bool fabric_connected,
                                       uint64_t bytes_done,
                                       uint64_t bytes_total,
                                       double live_mbps,
                                       const std::string& status,
                                       const std::string& notification,
                                       const std::string& error) {
    const Phase phase = derive_phase(busy, waiting, bytes_total, status, notification, error);
    phase_label_->SetLabel(phase_title(phase, fabric_connected));
    phase_label_->SetForegroundColour(phase_colour(phase));

    const bool show_progress = phase == Phase::Waiting
                               || phase == Phase::Transferring
                               || phase == Phase::Complete;
    bar_->Show(show_progress);
    percent_label_->Show(show_progress);
    bytes_label_->Show(show_progress);
    speed_label_->Show(show_progress);

    switch (phase) {
        case Phase::Waiting:
            bar_->SetActiveColour(kWarn);
            bar_->Pulse();
            percent_label_->SetLabel("");
            bytes_label_->SetLabel("");
            speed_label_->SetLabel("");
            message_label_->SetLabel(status.empty() ? "Waiting for the other side to respond…"
                                                    : status);
            message_label_->SetForegroundColour(kMuted);
            break;

        case Phase::Transferring: {
            const int pct = static_cast<int>((bytes_done * 100) / bytes_total);
            bar_->SetActiveColour(kAccent);
            bar_->SetFraction(static_cast<double>(bytes_done) / static_cast<double>(bytes_total));
            percent_label_->SetLabel(wxString::Format("%d%%", pct));
            percent_label_->SetForegroundColour(kAccent);
            bytes_label_->SetLabel(format_mib(bytes_done) + " of " + format_mib(bytes_total));
            std::ostringstream speed;
            speed << std::fixed << std::setprecision(1) << live_mbps << " MiB/s";
            speed_label_->SetLabel(speed.str());
            message_label_->SetLabel(status.empty() ? "Keep both apps open until finished."
                                                    : status);
            message_label_->SetForegroundColour(kMuted);
            break;
        }

        case Phase::Complete:
            bar_->SetActiveColour(kSuccess);
            bar_->SetFraction(1.0);
            percent_label_->SetLabel("100%");
            percent_label_->SetForegroundColour(kSuccess);
            if (bytes_total > 0) {
                bytes_label_->SetLabel(format_mib(bytes_total) + " transferred");
            } else {
                bytes_label_->SetLabel("");
            }
            speed_label_->SetLabel("");
            if (!notification.empty()) {
                message_label_->SetLabel(notification);
            } else if (!status.empty()) {
                message_label_->SetLabel(status);
            } else {
                message_label_->SetLabel("File delivered successfully.");
            }
            message_label_->SetForegroundColour(kSuccess);
            break;

        case Phase::Failed:
            percent_label_->SetLabel("");
            bytes_label_->SetLabel("");
            speed_label_->SetLabel("");
            message_label_->SetLabel(error.empty() ? "Something went wrong." : error);
            message_label_->SetForegroundColour(kError);
            break;

        case Phase::Idle:
        default:
            percent_label_->SetLabel("");
            bytes_label_->SetLabel("");
            speed_label_->SetLabel("");
            if (!error.empty()) {
                message_label_->SetLabel(error);
                message_label_->SetForegroundColour(kError);
            } else if (!notification.empty()) {
                message_label_->SetLabel(notification);
                message_label_->SetForegroundColour(kAccent);
            } else {
                message_label_->SetLabel("");
            }
            break;
    }
    Layout();
}
