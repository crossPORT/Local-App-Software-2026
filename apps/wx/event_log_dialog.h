#pragma once

#include <wx/wx.h>
#include <wx/timer.h>

class EventLogDialog : public wxDialog {
public:
    EventLogDialog(wxWindow* parent, int port_index);

private:
    void ReloadLog();
    void OnRefresh(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);
    void OnFilterChanged(wxCommandEvent& event);
    void OnAutoRefreshTimer(wxTimerEvent& event);

    int port_index_ = 0;
    wxStaticText* path_label_ = nullptr;
    wxTextCtrl* log_view_ = nullptr;
    wxCheckBox* port_filter_ = nullptr;
    wxCheckBox* auto_refresh_ = nullptr;
    wxTimer refresh_timer_;
};
