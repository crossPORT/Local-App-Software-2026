#include "session_handshake.h"

#include "usb_protocol.h"

namespace {

unsigned positive_or_default(int value, unsigned fallback) {
    return value > 0 ? static_cast<unsigned>(value) : fallback;
}

}  // namespace

HandshakeTiming handshake_timing_from_identity(const IdentityProfile& profile) {
    HandshakeTiming timing;
    timing.accept_ready_gap_ms =
        positive_or_default(profile.accept_ready_gap_ms, usb_protocol::kAcceptReadyGapMs);
    timing.accept_reply_delay_ms =
        profile.accept_reply_delay_ms > 0
            ? static_cast<unsigned>(profile.accept_reply_delay_ms)
            : timing.accept_ready_gap_ms * 2;
    timing.accept_timeout_sec =
        positive_or_default(profile.accept_timeout_sec, usb_protocol::kAcceptTimeoutSec);
    timing.ready_timeout_sec =
        positive_or_default(profile.ready_timeout_sec, usb_protocol::kReadyTimeoutSec);
    timing.session_header_timeout_ms = positive_or_default(profile.session_header_timeout_ms,
                                                           usb_protocol::kSessionHeaderTimeoutMs);
    timing.payload_header_timeout_ms = positive_or_default(profile.payload_header_timeout_ms,
                                                             usb_protocol::kPayloadHeaderTimeoutMs);

    const unsigned margin = usb_protocol::kAcceptReceiverMarginSec;
    timing.accept_dialog_sec =
        timing.accept_timeout_sec > margin ? timing.accept_timeout_sec - margin : 1;
    timing.handshake_poll_timeout_ms = 350;
    return timing;
}
