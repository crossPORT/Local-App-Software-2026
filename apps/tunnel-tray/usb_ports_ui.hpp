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

/** Present USB cables only (serial → Port). */
std::vector<UsbPortChoice> usb_port_choices();
std::vector<UsbPortChoice> sim_port_choices();

int fill_port_choice(wxChoice* choice, int preferred_port, bool enable, bool usb);
int fill_usb_port_choice(wxChoice* choice, int preferred_port, bool enable);

int selected_display_port(wxChoice* choice);
bool display_port_available(int display_port);
/** Sole available USB display port, or 0 if none/ambiguous. */
int sole_available_display_port();

}  // namespace tunnel_tray
