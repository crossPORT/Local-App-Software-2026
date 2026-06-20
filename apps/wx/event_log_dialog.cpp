#include "event_log_dialog.h"

#include "booth_log.h"

#include <sstream>
#include <wx/clipbrd.h>
#include <wx/sizer.h>

namespace {

const wxColour kBg(0x0f, 0x14, 0x19);
const wxColour kPanel(0x1a, 0x23, 0x32);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kLogBg(0x12, 0x18, 0x22);

enum {
    ID_RefreshLog = wxID_HIGHEST + 400,
    ID_CopyLog,
    ID_ClearLog,
    ID_PortFilter,
    ID_AutoRefresh,
    ID_LogRefreshTimer,
};

std::string filter_log_for_port(const std::string& text, int port_index) {
    const std::string needle = "[port=" + std::to_string(port_index) + "]";
    std::istringstream in(text);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(needle) != std::string::npos || line.find("[boot]") != std::string::npos) {
            out << line << '\n';
        }
    }
    const std::string filtered = out.str();
    return filtered.empty() ? "(no lines for this port yet)\n" : filtered;
}

}  // namespace

EventLogDialog::EventLogDialog(wxWindow* parent, int port_index)
    : wxDialog(parent,
               wxID_ANY,
               "Event Log",
               wxDefaultPosition,
               wxSize(640, 480),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , port_index_(port_index)
    , refresh_timer_(this, ID_LogRefreshTimer) {
    SetBackgroundColour(kBg);

    auto* root = new wxBoxSizer(wxVERTICAL);

    path_label_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
    path_label_->SetForegroundColour(kMuted);
    path_label_->Wrap(600);
    root->Add(path_label_, 0, wxEXPAND | wxALL, 10);

    auto* filter_row = new wxBoxSizer(wxHORIZONTAL);
    port_filter_ = new wxCheckBox(this, ID_PortFilter, "Only this app (port filter)");
    port_filter_->SetValue(true);
    port_filter_->SetForegroundColour(kText);
    auto_refresh_ = new wxCheckBox(this, ID_AutoRefresh, "Auto-refresh");
    auto_refresh_->SetValue(true);
    auto_refresh_->SetForegroundColour(kText);
    filter_row->Add(port_filter_, 0, wxRIGHT, 16);
    filter_row->Add(auto_refresh_, 0);
    root->Add(filter_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    log_view_ = new wxTextCtrl(this,
                               wxID_ANY,
                               "",
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxTE_RICH2);
    log_view_->SetBackgroundColour(kLogBg);
    log_view_->SetForegroundColour(kText);
    wxFont mono = log_view_->GetFont();
    mono.SetFamily(wxFONTFAMILY_TELETYPE);
    mono.SetPointSize(9);
    log_view_->SetFont(mono);
    root->Add(log_view_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* btn_row = new wxBoxSizer(wxHORIZONTAL);
    auto* refresh_btn = new wxButton(this, ID_RefreshLog, "Refresh");
    auto* copy_btn = new wxButton(this, ID_CopyLog, "Copy all");
    auto* clear_btn = new wxButton(this, ID_ClearLog, "Clear log");
    auto* close_btn = new wxButton(this, wxID_CLOSE, "Close");
    btn_row->Add(refresh_btn, 0, wxRIGHT, 6);
    btn_row->Add(copy_btn, 0, wxRIGHT, 6);
    btn_row->Add(clear_btn, 0, wxRIGHT, 6);
    btn_row->AddStretchSpacer(1);
    btn_row->Add(close_btn, 0);
    root->Add(btn_row, 0, wxEXPAND | wxALL, 10);

    Bind(wxEVT_BUTTON, &EventLogDialog::OnRefresh, this, ID_RefreshLog);
    Bind(wxEVT_BUTTON, &EventLogDialog::OnCopy, this, ID_CopyLog);
    Bind(wxEVT_BUTTON, &EventLogDialog::OnClear, this, ID_ClearLog);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        refresh_timer_.Stop();
        event.Skip();
    });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);
    Bind(wxEVT_CHECKBOX, &EventLogDialog::OnFilterChanged, this, ID_PortFilter);
    Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (auto_refresh_->GetValue()) {
            refresh_timer_.Start(1000);
        } else {
            refresh_timer_.Stop();
        }
        ReloadLog();
    }, ID_AutoRefresh);
    Bind(wxEVT_TIMER, &EventLogDialog::OnAutoRefreshTimer, this, ID_LogRefreshTimer);

    SetSizer(root);
    ReloadLog();
    refresh_timer_.Start(1000);
    CentreOnParent();
}

void EventLogDialog::ReloadLog() {
    const std::string path = booth_log_path();
    path_label_->SetLabel("Log file: " + path);

    std::string text = read_booth_log_tail(500);
    if (port_filter_->GetValue()) {
        text = filter_log_for_port(text, port_index_);
    }
    log_view_->ChangeValue(wxString::FromUTF8(text.c_str()));
    log_view_->ShowPosition(log_view_->GetLastPosition());
}

void EventLogDialog::OnRefresh(wxCommandEvent&) {
    ReloadLog();
}

void EventLogDialog::OnCopy(wxCommandEvent&) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(log_view_->GetValue()));
        wxTheClipboard->Close();
    }
}

void EventLogDialog::OnClear(wxCommandEvent&) {
    const int answer = wxMessageBox("Clear the shared event log file?",
                                    "Clear log",
                                    wxYES_NO | wxICON_QUESTION,
                                    this);
    if (answer == wxYES) {
        booth_log_clear();
        booth_log(port_index_, "log_cleared", "from Event Log dialog");
        ReloadLog();
    }
}

void EventLogDialog::OnFilterChanged(wxCommandEvent&) {
    ReloadLog();
}

void EventLogDialog::OnAutoRefreshTimer(wxTimerEvent&) {
    if (auto_refresh_->GetValue()) {
        ReloadLog();
    }
}
