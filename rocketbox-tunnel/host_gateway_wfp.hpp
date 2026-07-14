#pragma once

#include "expose_spec.hpp"

#include <cstdint>
#include <string>
#include <vector>

/** Opaque WFP session for RocketBox fabric-IP expose filter. */
struct WfpExposeSession;

WfpExposeSession* wfp_expose_open(int local_port);
void wfp_expose_close(WfpExposeSession* s);
/** ICMP permit + TCP/UDP allowlist to 10.64.0.N; empty list blocks host TCP/UDP. */
void wfp_expose_apply(WfpExposeSession* s, const std::vector<ExposeRule>& expose);
