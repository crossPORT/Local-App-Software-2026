#pragma once

#include "transfer_orchestrator.h"

#include <functional>
#include <wx/wx.h>

class IncomingDialog : public wxDialog {
public:
    using DecisionCallback = std::function<void(bool accepted)>;

    IncomingDialog(wxWindow* parent,
                   const PendingOffer& offer,
                   DecisionCallback on_decision);

private:
    void OnAccept(wxCommandEvent& event);
    void OnReject(wxCommandEvent& event);
    void OnCountdown(wxTimerEvent& event);
    void Decide(bool accepted, int end_code);

    DecisionCallback on_decision_;
    wxStaticText* countdown_label_ = nullptr;
    wxTimer countdown_timer_;
    int remaining_secs_ = 0;
    bool decided_ = false;
};
