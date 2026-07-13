#pragma once

#include <string>
#include <wx/dialog.h>

/** Confirm clearing EP4 switch to a peer (PWA ConfirmLinkReleaseDialog parity). */
class LinkReleaseDialog : public wxDialog {
public:
    LinkReleaseDialog(wxWindow* parent,
                      const std::string& peer_label,
                      int display_port,
                      bool waiting_for_accept);
};
