#pragma once

#include <string>
#include <vector>

constexpr int kFabricLegCount = 4;

/** User-facing port number (1–4) from internal fabric leg (0–3). */
int display_port_from_leg(int leg);

/** Primary UI label for a connected fabric port. */
std::string format_fabric_port_label(int leg);

/** Derive fabric leg (0–3) from cable USB serial — matches the PWA. */
int fabric_leg_from_serial(const std::string& serial);

/** Default remote-leg guess when an announce note omits port=. */
int default_remote_guess_leg(int my_leg);

/** The three fabric legs that are not {@a my_leg}, ascending order. */
std::vector<int> remote_fabric_legs(int my_leg);
