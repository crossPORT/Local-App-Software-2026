#include "usb_ports_ui.hpp"

#include "tunnel_proc.hpp"

#include <wx/choice.h>
#include <wx/string.h>

namespace tunnel_tray {

std::vector<UsbPortChoice> usb_port_choices() {
  std::vector<UsbPortChoice> out;
  for (const auto& p : rocketbox::list_present_ports()) {
    if (p.display_port < 1 || p.display_port > 4) continue;
    UsbPortChoice c;
    c.display_port = p.display_port;
    c.present = true;
    c.available = p.available;
    std::string lab = "Port " + std::to_string(p.display_port) + " - 10.64.0." +
                      std::to_string(p.display_port);
    if (!p.serial.empty()) {
      lab += " - " + (p.serial.size() > 8 ? p.serial.substr(p.serial.size() - 8) : p.serial);
    }
    lab += p.available ? " (USB)" : " (in use)";
    c.label = std::move(lab);
    out.push_back(std::move(c));
  }
  // While our tunnel holds the cable, libusb often hides it from the tray.
  // Keep the Port visible from live stats so the panel does not flash "No USB cable".
  for (int d = 1; d <= 4; ++d) {
    if (!live_tunnel_holds_port(d)) continue;
    bool have = false;
    for (const auto& c : out) {
      if (c.display_port == d) {
        have = true;
        break;
      }
    }
    if (have) continue;
    UsbPortChoice c;
    c.display_port = d;
    c.present = true;
    c.available = false;
    const auto rates = read_tunnel_rates(d);
    std::string lab = "Port " + std::to_string(d) + " - 10.64.0." + std::to_string(d);
    if (!rates.serial.empty()) {
      lab += " - " + (rates.serial.size() > 8 ? rates.serial.substr(rates.serial.size() - 8)
                                              : rates.serial);
    }
    lab += " (tunnel)";
    c.label = std::move(lab);
    out.push_back(std::move(c));
  }
  return out;
}

std::vector<UsbPortChoice> sim_port_choices() {
  std::vector<UsbPortChoice> out;
  for (int d = 1; d <= 4; ++d) {
    UsbPortChoice c;
    c.display_port = d;
    c.present = true;
    c.available = true;
    c.label = "Port " + std::to_string(d) + " - 10.64.0." + std::to_string(d) + " (sim)";
    out.push_back(std::move(c));
  }
  return out;
}

int fill_usb_port_choice(wxChoice* choice, int preferred_port, bool enable,
                         std::vector<int>* ports_out) {
  return fill_port_choice(choice, preferred_port, enable, true, ports_out);
}

int fill_port_choice(wxChoice* choice, int preferred_port, bool enable, bool usb,
                     std::vector<int>* ports_out) {
  choice->Clear();
  if (ports_out) ports_out->clear();
  auto ports = usb ? usb_port_choices() : sim_port_choices();
  // Right after Enable, tunnel claims USB so libusb is empty before stats exist.
  // Keep the known Port in the dropdown (status already shows it).
  if (ports.empty() && usb && preferred_port >= 1 && preferred_port <= 4) {
    UsbPortChoice c;
    c.display_port = preferred_port;
    c.present = true;
    c.available = false;
    c.label = "Port " + std::to_string(preferred_port) + " - 10.64.0." +
              std::to_string(preferred_port) + " (tunnel)";
    ports.push_back(std::move(c));
  }
  if (ports.empty()) {
    choice->Append(wxT("No USB cable"));
    if (ports_out) ports_out->push_back(0);
    choice->SetSelection(0);
    choice->Enable(false);
    return 0;
  }
  int sel = 0;
  for (size_t i = 0; i < ports.size(); ++i) {
    choice->Append(wxString(ports[i].label.c_str(), wxConvUTF8));
    if (ports_out) ports_out->push_back(ports[i].display_port);
    if (ports[i].display_port == preferred_port) sel = static_cast<int>(i);
  }
  if (usb && (preferred_port < 1 || preferred_port > 4)) {
    int n_avail = 0, sole = 0;
    for (size_t i = 0; i < ports.size(); ++i) {
      if (ports[i].available) {
        ++n_avail;
        sole = static_cast<int>(i);
      }
    }
    if (n_avail == 1) sel = sole;
  }
  choice->SetSelection(sel);
  choice->Enable(enable && (!usb || ports.size() > 1));
  return ports[static_cast<size_t>(sel)].display_port;
}

int selected_display_port(const wxChoice* choice, const std::vector<int>& ports) {
  const int idx = choice->GetSelection();
  if (idx < 0 || static_cast<size_t>(idx) >= ports.size()) return 0;
  return ports[static_cast<size_t>(idx)];
}

bool display_port_available(int display_port) {
  for (const auto& p : rocketbox::list_present_ports()) {
    if (p.display_port == display_port) return p.available;
  }
  return false;
}

int sole_available_display_port() {
  int n = 0, sole = 0;
  for (const auto& p : rocketbox::list_present_ports()) {
    if (p.display_port < 1 || p.display_port > 4 || !p.available) continue;
    ++n;
    sole = p.display_port;
  }
  return n == 1 ? sole : 0;
}

int count_present_usb_ports() {
  int n = 0;
  for (const auto& p : rocketbox::list_present_ports()) {
    if (p.display_port >= 1 && p.display_port <= 4) ++n;
  }
  return n;
}

}  // namespace tunnel_tray
