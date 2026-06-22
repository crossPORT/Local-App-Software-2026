#pragma once

#include "identity_profile.h"

struct HandshakeTiming {
    unsigned accept_ready_gap_ms = 400;
    unsigned accept_reply_delay_ms = 800;
    unsigned accept_timeout_sec = 60;
    unsigned accept_dialog_sec = 57;
    unsigned ready_timeout_sec = 20;
    unsigned session_header_timeout_ms = 2000;
    unsigned handshake_poll_timeout_ms = 350;
    unsigned payload_header_timeout_ms = 15000;
};

HandshakeTiming handshake_timing_from_identity(const IdentityProfile& profile);
