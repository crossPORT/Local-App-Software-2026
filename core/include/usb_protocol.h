#pragma once

#include <cstddef>
#include <cstdint>

namespace usb_protocol {

constexpr uint16_t kVendorId = 0x1772;
constexpr uint16_t kProductId = 0x0006;

constexpr int kInterface = 0;

constexpr unsigned char kEndpointDataOut = 0x02;
constexpr unsigned char kEndpointDataIn = 0x81;

// 32-byte file header — magic "ROCKETBX"
constexpr char kHeaderMagic[8] = {'R', 'O', 'C', 'K', 'E', 'T', 'B', 'X'};
constexpr std::size_t kHeaderSize = 32;

constexpr std::size_t kChunkSize = 4 * 1024 * 1024;
constexpr std::size_t kQueueDepth = 32;
constexpr std::size_t kPoolSize = kQueueDepth + 2;

// Default per-chunk payload timeout (backstop for a stream that stalls while the
// device stays enumerated). This is per 4 MB chunk / per 32-byte header send, so
// 8s is a generous backstop (>=0.5 MB/s) while still surfacing a wedged routing
// state or missing receiver fast instead of hanging. A real cable pull fails
// fast via transfer status. Override per-deployment via config
// (transfer_timeout_ms) → set_payload_timeout_ms().
constexpr unsigned kFileTimeoutMs = 8000;
constexpr unsigned kSessionFileTimeoutMs = 8000;
constexpr unsigned kSessionHeaderTimeoutMs = 2000;
// Wait for the payload header after sending "ready". Bounded so a cable pulled
// between the handshake and the payload fails fast instead of hanging ~2 min.
constexpr unsigned kPayloadHeaderTimeoutMs = 15000;
constexpr unsigned kHandshakeTimeoutSec = 15;

// How long the sender waits for the receiver to ACCEPT an offer. This is a human
// decision in ask-first mode, so it must be generous. The sender shows a live
// countdown; the receiver's prompt counts down slightly faster (margin below) so
// it resolves before the sender gives up, avoiding a late-accept desync.
constexpr unsigned kAcceptTimeoutSec = 60;
constexpr unsigned kAcceptReceiverMarginSec = 3;
// After the receiver accepts, how long the sender waits for the receiver to be
// ready + the payload handshake to start before giving up.
constexpr unsigned kReadyTimeoutSec = 20;
// Pause between accept and ready replies so the sender's session listener can
// re-arm its USB read before the second message arrives (fabric does not buffer).
constexpr unsigned kAcceptReadyGapMs = 400;
// Receiver must wait longer than the sender's post-offer listener re-arm gap
// before sending accept, or the accept is lost on the no-buffer fabric path.
constexpr unsigned kAcceptReplyDelayMs = kAcceptReadyGapMs * 2;

}  // namespace usb_protocol
