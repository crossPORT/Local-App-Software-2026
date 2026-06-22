/** Mirrors core/include/usb_protocol.h session/payload timeouts. */
import { HANDSHAKE_DEFAULTS } from './session_handshake';

export const SESSION_HEADER_TIMEOUT_MS = HANDSHAKE_DEFAULTS.session_header_timeout_ms;
export const PAYLOAD_HEADER_TIMEOUT_MS = HANDSHAKE_DEFAULTS.payload_header_timeout_ms;
export const ACCEPT_TIMEOUT_SEC = HANDSHAKE_DEFAULTS.accept_timeout_sec;
export const ACCEPT_RECEIVER_MARGIN_SEC = HANDSHAKE_DEFAULTS.accept_receiver_margin_sec;
export const READY_TIMEOUT_SEC = HANDSHAKE_DEFAULTS.ready_timeout_sec;
export const ACCEPT_READY_GAP_MS = HANDSHAKE_DEFAULTS.accept_ready_gap_ms;
export const ACCEPT_REPLY_DELAY_MS = HANDSHAKE_DEFAULTS.accept_reply_delay_ms;
export const MAX_SESSION_FILE_BYTES = 65536;

export const ACCEPT_DIALOG_SEC = ACCEPT_TIMEOUT_SEC - ACCEPT_RECEIVER_MARGIN_SEC;
export const TRANSFER_DONE_DISMISS_MS = 2000;
