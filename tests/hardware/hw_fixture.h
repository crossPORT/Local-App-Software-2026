#pragma once

#include <libusb.h>

// Shared libusb context for the hardware suite, created in hardware_main.cpp
// after confirming two RocketBox devices are present.
extern libusb_context* g_hw_ctx;
