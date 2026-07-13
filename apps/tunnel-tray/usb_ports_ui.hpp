#pragma once

#include "rocketbox/port_probe.h"

#include <string>
#include <vector>

class wxChoice;

namespace tunnel_tray {

struct UsbPortChoice {
  int display_port = 0;
  bool present = false;
  bool available = false;
  std::string label;
};

std::vector<UsbPortChoice> usb_port_choices();
std::vector<UsbPortChoice> sim_port_choices();

/** Fill labels; ports_out[i] = display port for item i (0 = none). Returns selection. */
int fill_port_choice(wxChoice* choice, int preferred_port, bool enable, bool usb,
                     std::vector<int>* ports_out);
int fill_usb_port_choice(wxChoice* choice, int preferred_port, bool enable,
                         std::vector<int>* ports_out);

int selected_display_port(const wxChoice* choice, const std::vector<int>& ports);
bool display_port_available(int display_port);
int sole_available_display_port();

/** Present cables (may be busy). Empty = none plugged in. */
int count_present_usb_ports();

}  // namespace tunnel_tray
