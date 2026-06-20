#include "incoming_dialog.h"

#include "usb_protocol.h"

#include <iomanip>
#include <sstream>
#include <wx/sizer.h>

namespace {

const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);

std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    if (bytes < 1024 * 1024) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1)
            << (static_cast<double>(bytes) / 1024.0) << " KiB";
        return out.str();
    }
    if (bytes < 1024ull * 1024 * 1024) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB";
        return out.str();
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)) << " GiB";
    return out.str();
}

}  // namespace

enum { ID_Accept = wxID_HIGHEST + 200, ID_Reject };

IncomingDialog::IncomingDialog(wxWindow* parent,
                               const PendingOffer& offer,
                               DecisionCallback on_decision)
    : wxDialog(parent,
               wxID_ANY,
               "New transfer request",
               wxDefaultPosition,
               wxSize(420, 320),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
    , on_decision_(std::move(on_decision))
    , countdown_timer_(this) {
    const auto& msg = offer.message;
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, "NEW TRANSFER REQUEST");
    wxFont title_font = title->GetFont();
    title_font.SetPointSize(12);
    title_font.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(title_font);
    title->SetForegroundColour(kText);
    root->Add(title, 0, wxALL, 12);

    root->Add(new wxStaticText(this, wxID_ANY, "From: " + msg.from_name + "  ·  " + msg.team),
              0, wxLEFT | wxRIGHT, 12);
    root->Add(new wxStaticText(this, wxID_ANY, "File: " + msg.payload_name),
              0, wxLEFT | wxRIGHT | wxTOP, 6);
    std::string size_line = "Size: " + format_bytes(msg.total_bytes);
    if (msg.file_count > 1) {
        size_line += " (" + std::to_string(msg.file_count) + " files)";
    }
    root->Add(new wxStaticText(this, wxID_ANY, size_line),
              0, wxLEFT | wxRIGHT | wxTOP, 6);
    if (!msg.note.empty()) {
        auto* note = new wxStaticText(this, wxID_ANY, "Note: " + msg.note);
        note->Wrap(380);
        root->Add(note, 0, wxLEFT | wxRIGHT | wxTOP, 6);
    }

    remaining_secs_ = static_cast<int>(usb_protocol::kAcceptTimeoutSec)
        - static_cast<int>(usb_protocol::kAcceptReceiverMarginSec);
    if (remaining_secs_ < 1) {
        remaining_secs_ = 1;
    }
    countdown_label_ = new wxStaticText(
        this, wxID_ANY,
        "Auto-declines in " + std::to_string(remaining_secs_) + "s");
    countdown_label_->SetForegroundColour(kMuted);
    root->Add(countdown_label_, 0, wxLEFT | wxRIGHT | wxTOP, 12);

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* accept = new wxButton(this, ID_Accept, "ACCEPT");
    auto* reject = new wxButton(this, ID_Reject, "REJECT");
    row->Add(accept, 1, wxRIGHT, 6);
    row->Add(reject, 1);
    root->Add(row, 0, wxEXPAND | wxALL, 12);

    Bind(wxEVT_BUTTON, &IncomingDialog::OnAccept, this, ID_Accept);
    Bind(wxEVT_BUTTON, &IncomingDialog::OnReject, this, ID_Reject);
    Bind(wxEVT_TIMER, &IncomingDialog::OnCountdown, this);
    SetSizer(root);

    countdown_timer_.Start(1000);
}

void IncomingDialog::Decide(bool accepted, int end_code) {
    if (decided_) {
        return;
    }
    decided_ = true;
    countdown_timer_.Stop();
    if (on_decision_) {
        on_decision_(accepted);
    }
    EndModal(end_code);
}

void IncomingDialog::OnAccept(wxCommandEvent&) {
    Decide(true, wxID_OK);
}

void IncomingDialog::OnReject(wxCommandEvent&) {
    Decide(false, wxID_CANCEL);
}

void IncomingDialog::OnCountdown(wxTimerEvent&) {
    --remaining_secs_;
    if (remaining_secs_ <= 0) {
        Decide(false, wxID_CANCEL);
        return;
    }
    if (countdown_label_) {
        countdown_label_->SetLabel(
            "Auto-declines in " + std::to_string(remaining_secs_) + "s");
    }
}
