#pragma once

#include "listen_ports.hpp"

#include <vector>

#include <wx/string.h>

class wxCheckListBox;

namespace tunnel_tray {

struct ExposeRow {
  Endpoint ep;
  wxString label;
};

std::vector<ExposeRow> build_expose_rows(const std::vector<Endpoint>& sel);
void fill_expose_list(wxCheckListBox* list, const std::vector<ExposeRow>& rows,
                      const std::vector<Endpoint>& sel);

}  // namespace tunnel_tray
