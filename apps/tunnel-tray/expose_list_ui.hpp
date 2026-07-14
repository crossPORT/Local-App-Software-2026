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

bool expose_selection_equal(const std::vector<Endpoint>& a, const std::vector<Endpoint>& b);

std::vector<ExposeRow> build_expose_rows(const std::vector<Endpoint>& sel);
void fill_expose_list(wxCheckListBox* list, const std::vector<ExposeRow>& rows,
                      const std::vector<Endpoint>& sel);

}  // namespace tunnel_tray
