#include "transfer_progress_panel.h"

#include "link_status.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {

const wxColour kPanel(0x1a, 0x23, 0x32);
const wxColour kStatusBar(0x0f, 0x16, 0x20);
const wxColour kStatusLabel(0x88, 0x99, 0xaa);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kSuccess(0x3d, 0xdb, 0x8a);
const wxColour kError(0xff, 0x6b, 0x6b);
const wxColour kWarn(0xff, 0x9f, 0x43);
const wxColour kTrack(0x24, 0x30, 0x42);
const wxColour kCard(0x12, 0x18, 0x22);

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

std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024ull) {
        return std::to_string(bytes) + " B";
    }
    if (bytes >= 1024ull * 1024 * 1024) {
        const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        std::ostringstream out;
        if (gib >= 100.0) {
            out << std::fixed << std::setprecision(0) << gib;
        } else if (gib >= 10.0) {
            out << std::fixed << std::setprecision(1) << gib;
        } else {
            out << std::fixed << std::setprecision(2) << gib;
        }
        out << " GiB";
        return out.str();
    }
    if (bytes < 1024ull * 10) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(bytes >= 1024 ? 1 : 0)
            << (static_cast<double>(bytes) / 1024.0) << " KiB";
        return out.str();
    }
    return format_mib(bytes);
}

std::string format_mbps(double mbps) {
    const std::string rate = format_mbps_rate(mbps);
    if (!rate.empty()) {
        return rate;
    }
    return "0 MiB/s";
}

wxString label_and_size(const std::string& label, uint64_t bytes) {
    if (label.empty()) {
        return wxString::FromUTF8(format_bytes(bytes).c_str());
    }
    return wxString::FromUTF8((label + " · " + format_bytes(bytes)).c_str());
}

std::string format_final_speed(double peak_mbps, double result_mbps, bool /*booth_display*/) {
    const double speed = result_mbps > 0.0 ? result_mbps
                                           : (peak_mbps > 0.0 ? peak_mbps : 0.0);
    if (speed <= 0.0) {
        return {};
    }
    std::string out = format_mbps(speed);
    return out;
}

std::string format_speed_line(double live_mbps,
                              double peak_mbps,
                              double result_mbps,
                              bool booth_display,
                              bool transferring,
                              bool sending) {
    if (transferring) {
        std::string line = transfer_live_message(sending, live_mbps);
        return line;
    }
    return format_final_speed(peak_mbps, result_mbps, booth_display);
}

bool transfer_is_sending(const std::string& status) {
    return status.rfind("Sending ", 0) == 0;
}

bool transfer_is_receiving(const std::string& status) {
    return status.rfind("Receiving ", 0) == 0 || status.rfind("Waiting for ", 0) == 0;
}

bool looks_complete(const std::string& status, const std::string& notification) {
    auto has = [](const std::string& hay, const char* needle) {
        return hay.find(needle) != std::string::npos;
    };
    return has(notification, "Sent at") || has(notification, "Received at")
           || has(status, "Sent at") || has(status, "Received at")
           || has(notification, "Sent") || has(notification, "Received");
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
    if (busy && (transfer_is_sending(status) || transfer_is_receiving(status))) {
        return Phase::Transferring;
    }
    if (!busy && looks_complete(status, notification)) {
        return Phase::Complete;
    }
    return Phase::Idle;
}

wxString phase_title(Phase phase, bool fabric_connected, bool has_peers, const std::string& status) {
    switch (phase) {
        case Phase::Idle:
            if (!fabric_connected) {
                return "Not connected";
            }
            return has_peers ? "Ready" : "Waiting for peers…";
        case Phase::Waiting:
            return "Waiting for a file…";
        case Phase::Transferring:
            return transfer_is_receiving(status) ? "Receiving file…" : "Sending file…";
        case Phase::Complete:
            return "Done";
        case Phase::Failed:
            return "Transfer failed";
    }
    return has_peers ? "Ready" : "Waiting for peers…";
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
            return kWarn;
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
    SetBackgroundColour(kStatusBar);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* head_row = new wxBoxSizer(wxHORIZONTAL);
    head_row->Add(MakeLabel(this, "Status", kStatusLabel, 10, wxFONTWEIGHT_BOLD),
                  0,
                  wxALIGN_CENTER_VERTICAL);
    head_row->AddStretchSpacer(1);
    phase_label_ = MakeLabel(this, "Not connected", kMuted, 11, wxFONTWEIGHT_NORMAL);
    head_row->Add(phase_label_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(head_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

    auto* bar_row = new wxBoxSizer(wxHORIZONTAL);
    bar_ = new ThemeProgressBar(this);
    bar_->SetBackgroundColour(kStatusBar);
    bar_row->Add(bar_, 1, wxALIGN_CENTER_VERTICAL);
    percent_label_ = MakeLabel(this, "", kText, 12, wxFONTWEIGHT_BOLD);
    percent_label_->SetMinSize(wxSize(48, -1));
    bar_row->Add(percent_label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    bar_row_item_ = root->Add(bar_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);

    bytes_label_ = MakeLabel(this, "", kText, 11);
    bytes_label_->Wrap(500);
    bytes_item_ = root->Add(bytes_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 4);

    speed_label_ = MakeLabel(this, "", kAccent, 12, wxFONTWEIGHT_BOLD);
    speed_item_ = root->Add(speed_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 4);

    message_label_ = MakeLabel(this, "", kMuted, 11);
    message_label_->Wrap(520);
    message_item_ = root->Add(message_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 10);

    reset_row_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    reset_row_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    reset_row_->SetBackgroundColour(kStatusBar);
    reset_row_->SetMinSize(wxSize(-1, 36));
    reset_row_->SetCursor(wxCursor(wxCURSOR_HAND));
    auto* reset_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto* reset_label = MakeLabel(reset_row_, "Reset connection", kMuted, 11);
    reset_sizer->AddStretchSpacer();
    reset_sizer->Add(reset_label, 0, wxALIGN_CENTER_VERTICAL);
    reset_sizer->AddStretchSpacer();
    reset_row_->SetSizer(reset_sizer);
    reset_row_->Bind(wxEVT_PAINT, [this](wxPaintEvent& event) {
        wxPaintDC dc(reset_row_);
        const wxSize sz = reset_row_->GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) {
            return;
        }
        dc.SetPen(wxPen(kTrack));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRoundedRectangle(0, 0, sz.x - 1, sz.y - 1, 8);
        event.Skip();
    });
    reset_row_->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
        event.StopPropagation();
        if (on_reset_) {
            on_reset_();
        }
    });
    reset_item_ = root->Add(reset_row_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    {
        wxFont idle = phase_label_->GetFont();
        idle.SetPointSize(11);
        idle.SetWeight(wxFONTWEIGHT_NORMAL);
        phase_label_idle_font_ = idle;
        wxFont active = phase_label_->GetFont();
        active.SetPointSize(13);
        active.SetWeight(wxFONTWEIGHT_BOLD);
        phase_label_active_font_ = active;
    }

    root_sizer_ = root;
    SetSizer(root);
    bar_row_item_->Show(false);
    bytes_item_->Show(false);
    speed_item_->Show(false);
    message_item_->Show(false);
    reset_item_->Show(false);
    SetMinSize(wxSize(-1, 36));
}

void TransferProgressPanel::SetResetHandler(std::function<void()> handler) {
    on_reset_ = std::move(handler);
}

void TransferProgressPanel::RelayoutAncestors() {
    Layout();
    for (wxWindow* ancestor = GetParent(); ancestor != nullptr; ancestor = ancestor->GetParent()) {
        ancestor->Layout();
    }
}

void TransferProgressPanel::ApplyState(bool busy,
                                       bool waiting,
                                       bool fabric_connected,
                                       bool has_peers,
                                       uint64_t bytes_done,
                                       uint64_t bytes_total,
                                       double live_mbps,
                                       double peak_mbps,
                                       double result_mbps,
                                       double booth_display_mib_s,
                                       const std::string& transfer_label,
                                       const std::string& status,
                                       const std::string& notification,
                                       const std::string& error) {
    const Phase phase = derive_phase(busy, waiting, bytes_total, status, notification, error);
    const bool booth_display = booth_display_mib_s > 0.0;
    const bool sending = transfer_is_sending(status);
    const bool receiving = transfer_is_receiving(status);
    const bool outbound = sending || !receiving;
    phase_label_->SetLabel(phase_title(phase, fabric_connected, has_peers, status));
    phase_label_->SetForegroundColour(phase_colour(phase));
    if (phase == Phase::Idle || phase == Phase::Failed) {
        phase_label_->SetFont(phase_label_idle_font_);
    } else {
        phase_label_->SetFont(phase_label_active_font_);
    }
    if (phase == Phase::Failed) {
        phase_label_->SetLabel("Transfer failed");
        phase_label_->SetForegroundColour(kWarn);
    }

    const bool show_progress = phase == Phase::Transferring || phase == Phase::Complete;
    const bool show_speed = phase == Phase::Transferring;
    const bool show_reset = phase == Phase::Failed;

    if (root_sizer_) {
        bar_row_item_->Show(show_progress);
        bytes_item_->Show(show_progress);
        speed_item_->Show(show_speed);
        reset_item_->Show(show_reset);
        int min_h = 36;
        if (show_progress) {
            min_h = show_reset ? 120 : 96;
        } else if (show_reset) {
            min_h = 72;
        } else if (phase == Phase::Waiting) {
            min_h = 56;
        }
        SetMinSize(wxSize(-1, min_h));
    }

    switch (phase) {
        case Phase::Waiting:
            bar_->SetActiveColour(kWarn);
            bar_->Pulse();
            percent_label_->SetLabel("");
            if (bytes_total > 0) {
                bytes_label_->SetLabel(label_and_size(transfer_label, bytes_total));
            } else {
                bytes_label_->SetLabel("");
            }
            speed_label_->SetLabel("");
            message_label_->SetLabel(status.empty() ? "Waiting for the other side to respond..."
                                                    : status);
            message_label_->SetForegroundColour(kMuted);
            break;

        case Phase::Transferring: {
            const int pct = bytes_total > 0
                                ? static_cast<int>((bytes_done * 100) / bytes_total)
                                : 0;
            bar_->SetActiveColour(kAccent);
            bar_->SetFraction(bytes_total > 0
                                  ? static_cast<double>(bytes_done) / static_cast<double>(bytes_total)
                                  : 0.0);
            percent_label_->SetLabel(wxString::Format("%d%%", pct));
            percent_label_->SetForegroundColour(kAccent);
            if (!transfer_label.empty() && bytes_total > 0) {
                bytes_label_->SetLabel(
                    wxString::FromUTF8((transfer_label + " · " + format_bytes(bytes_done)
                                        + " of " + format_bytes(bytes_total))
                                           .c_str()));
            } else if (bytes_total > 0) {
                bytes_label_->SetLabel(format_bytes(bytes_done) + " of "
                                     + format_bytes(bytes_total));
            } else {
                bytes_label_->SetLabel(wxString::FromUTF8(transfer_label.c_str()));
            }
            {
                const std::string speed = format_speed_line(
                    live_mbps, peak_mbps, result_mbps, booth_display, true, outbound);
                speed_label_->SetLabel(wxString::FromUTF8(speed.c_str()));
            }
            speed_label_->SetForegroundColour(kAccent);
            message_label_->SetLabel("");
            break;
        }

        case Phase::Complete:
            bar_->SetActiveColour(kSuccess);
            bar_->SetFraction(1.0);
            percent_label_->SetLabel("100%");
            percent_label_->SetForegroundColour(kSuccess);
            if (bytes_total > 0) {
                if (!transfer_label.empty()) {
                    bytes_label_->SetLabel(
                        wxString::FromUTF8((transfer_label + " · " + format_bytes(bytes_total))
                                               .c_str()));
                } else {
                    bytes_label_->SetLabel(format_bytes(bytes_total) + " transferred");
                }
            } else if (!transfer_label.empty()) {
                bytes_label_->SetLabel(wxString::FromUTF8(transfer_label.c_str()));
            } else {
                bytes_label_->SetLabel("");
            }
            if (!notification.empty()) {
                message_label_->SetLabel(wxString::FromUTF8(notification.c_str()));
            } else if (!status.empty()) {
                message_label_->SetLabel(wxString::FromUTF8(status.c_str()));
            } else {
                message_label_->SetLabel("File delivered successfully.");
            }
            message_label_->SetForegroundColour(kSuccess);
            break;

        case Phase::Failed: {
            percent_label_->SetLabel("");
            bytes_label_->SetLabel("");
            speed_label_->SetLabel("");
            const std::string detail = !error.empty() ? error : status;
            if (!detail.empty()) {
                message_label_->SetLabel(wxString::FromUTF8(detail.c_str()));
                message_label_->SetForegroundColour(kWarn);
            } else {
                message_label_->SetLabel("");
            }
            break;
        }

        case Phase::Idle:
        default:
            percent_label_->SetLabel("");
            bytes_label_->SetLabel("");
            speed_label_->SetLabel("");
            if (!notification.empty()) {
                message_label_->SetLabel(notification);
                message_label_->SetForegroundColour(kAccent);
            } else {
                message_label_->SetLabel("");
            }
            break;
    }

    if (root_sizer_) {
        const wxString message_text = message_label_->GetLabel();
        const bool message_visible =
            !message_text.IsEmpty()
            && (phase == Phase::Waiting || phase == Phase::Failed || phase == Phase::Complete
                || phase == Phase::Idle);
        message_item_->Show(message_visible);
    }

    RelayoutAncestors();
}
