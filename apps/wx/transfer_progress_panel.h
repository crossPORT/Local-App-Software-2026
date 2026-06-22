#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <wx/wx.h>

class ThemeProgressBar;

class TransferProgressPanel : public wxPanel {
public:
    explicit TransferProgressPanel(wxWindow* parent);

    void ApplyState(bool busy,
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
                    const std::string& error);

    void SetResetHandler(std::function<void()> handler);

private:
    void RelayoutAncestors();

    std::function<void()> on_reset_;
    wxPanel* reset_row_ = nullptr;
    wxSizerItem* reset_item_ = nullptr;

    wxSizer* root_sizer_ = nullptr;
    wxSizerItem* bar_row_item_ = nullptr;
    wxSizerItem* bytes_item_ = nullptr;
    wxSizerItem* speed_item_ = nullptr;
    wxSizerItem* message_item_ = nullptr;
    wxStaticText* phase_label_ = nullptr;
    wxStaticText* percent_label_ = nullptr;
    ThemeProgressBar* bar_ = nullptr;
    wxStaticText* bytes_label_ = nullptr;
    wxStaticText* speed_label_ = nullptr;
    wxStaticText* message_label_ = nullptr;
    wxFont phase_label_idle_font_;
    wxFont phase_label_active_font_;
};
