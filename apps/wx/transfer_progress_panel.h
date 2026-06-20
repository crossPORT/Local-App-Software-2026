#pragma once

#include <cstdint>
#include <string>
#include <wx/wx.h>

class ThemeProgressBar;

class TransferProgressPanel : public wxPanel {
public:
    explicit TransferProgressPanel(wxWindow* parent);

    void ApplyState(bool busy,
                    bool waiting,
                    bool fabric_connected,
                    uint64_t bytes_done,
                    uint64_t bytes_total,
                    double live_mbps,
                    const std::string& status,
                    const std::string& notification,
                    const std::string& error);

private:
    wxStaticText* phase_label_ = nullptr;
    wxStaticText* percent_label_ = nullptr;
    ThemeProgressBar* bar_ = nullptr;
    wxStaticText* bytes_label_ = nullptr;
    wxStaticText* speed_label_ = nullptr;
    wxStaticText* message_label_ = nullptr;
};
