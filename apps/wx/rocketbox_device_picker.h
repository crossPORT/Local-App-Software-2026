#pragma once

#include "usb_transfer.h"

#include <vector>
#include <wx/wx.h>

// Returns fabric sort index (0..N-1), or -1 when none / cancelled.
int pick_usb_port_index(wxWindow* parent, const std::vector<RocketBoxUsbDevice>& devices);
