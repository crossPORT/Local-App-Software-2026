#include "expose_list_ui.hpp"

#include "expose_presets.hpp"

#include <algorithm>
#include <map>
#include <set>

#include <wx/checklst.h>

namespace tunnel_tray {
namespace {

bool selected(const std::vector<Endpoint>& sel, const Endpoint& ep) {
  return std::find(sel.begin(), sel.end(), ep) != sel.end();
}

}  // namespace

bool expose_selection_equal(const std::vector<Endpoint>& a, const std::vector<Endpoint>& b) {
  if (a.size() != b.size()) return false;
  std::vector<Endpoint> sa = a, sb = b;
  std::sort(sa.begin(), sa.end());
  std::sort(sb.begin(), sb.end());
  return sa == sb;
}

std::vector<ExposeRow> build_expose_rows(const std::vector<Endpoint>& sel) {
  const auto live = scan_listening_services();
  std::map<Endpoint, std::string> proc_by_ep;
  std::set<Endpoint> live_set;
  for (const auto& s : live) {
    live_set.insert(s.ep);
    if (!s.process.empty()) proc_by_ep[s.ep] = s.process;
  }
  std::vector<ExposeRow> rows;
  auto add = [&](const Endpoint& ep, bool down) {
    const std::string proc = down ? std::string{} : proc_by_ep[ep];
    rows.push_back({ep, wxString(endpoint_menu_label(ep, down, proc).c_str(), wxConvUTF8)});
  };
  for (const auto& ep : expose_presets()) {
    if (!selected(sel, ep) && live_set.count(ep) == 0) add(ep, true);
  }
  for (const auto& ep : sel) add(ep, live_set.count(ep) == 0);
  std::vector<Endpoint> tcp, udp;
  for (const auto& s : live) {
    if (selected(sel, s.ep)) continue;
    (s.ep.proto == Proto::Tcp ? tcp : udp).push_back(s.ep);
  }
  auto by_port = [](const Endpoint& a, const Endpoint& b) { return a.port < b.port; };
  std::sort(tcp.begin(), tcp.end(), by_port);
  std::sort(udp.begin(), udp.end(), by_port);
  for (const auto& ep : tcp) add(ep, false);
  for (const auto& ep : udp) add(ep, false);
  return rows;
}

void fill_expose_list(wxCheckListBox* list, const std::vector<ExposeRow>& rows,
                      const std::vector<Endpoint>& sel) {
  list->Clear();
  for (size_t i = 0; i < rows.size(); ++i) {
    list->Append(rows[i].label);
    if (selected(sel, rows[i].ep)) list->Check(static_cast<int>(i));
  }
}

}  // namespace tunnel_tray
