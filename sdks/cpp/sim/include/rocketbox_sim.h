#pragma once

#include "usb_transfer.h"

#include <cstdint>
#include <string>
#include <vector>

// In-process USB sim (sdks/cpp/sim): two linked ports (0 ↔ 1) with ROCKETBX framing.
// Enable with ROCKETBOX_SIM=1 or rocketbox_sim_set_enabled(true) (tests).
// Session/tools link rocketbox_usb_sim when sim support is needed.

bool rocketbox_sim_enabled();
void rocketbox_sim_set_enabled(bool enabled);
void rocketbox_sim_reset();

TransferResult rocketbox_sim_send_file(const std::string& path,
                                    int port_index,
                                    ProgressCallback progress_cb,
                                    unsigned timeout_ms);

TransferResult rocketbox_sim_receive_file(const std::string& out_path,
                                       int port_index,
                                       ProgressCallback progress_cb,
                                       unsigned header_timeout_ms);

TransferResult rocketbox_sim_loopback(const std::string& path,
                                   int send_port_index,
                                   int recv_port_index,
                                   ProgressCallback progress_cb);

int rocketbox_sim_count_devices();
bool rocketbox_sim_port_available(int port_index);
bool rocketbox_sim_device_bus_addr(int port_index, uint8_t* bus_out, uint8_t* addr_out);
std::vector<RocketBoxUsbDevice> rocketbox_sim_list_devices();
