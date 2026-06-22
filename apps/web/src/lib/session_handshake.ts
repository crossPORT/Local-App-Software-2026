import type { IdentityProfile } from './types';

export const HANDSHAKE_DEFAULTS = {
  accept_ready_gap_ms: 400,
  accept_reply_delay_ms: 800,
  accept_timeout_sec: 60,
  accept_receiver_margin_sec: 3,
  ready_timeout_sec: 20,
  session_header_timeout_ms: 2000,
  /** Short IN poll while waiting for accept/ready (fabric does not buffer). */
  handshake_poll_timeout_ms: 350,
  payload_header_timeout_ms: 15000,
} as const;

export interface HandshakeTiming {
  accept_ready_gap_ms: number;
  accept_reply_delay_ms: number;
  accept_timeout_sec: number;
  accept_dialog_sec: number;
  ready_timeout_sec: number;
  session_header_timeout_ms: number;
  handshake_poll_timeout_ms: number;
  payload_header_timeout_ms: number;
}

type HandshakeIdentity = Pick<
  IdentityProfile,
  | 'accept_ready_gap_ms'
  | 'accept_reply_delay_ms'
  | 'accept_timeout_sec'
  | 'ready_timeout_sec'
  | 'session_header_timeout_ms'
  | 'payload_header_timeout_ms'
>;

export function handshakeTimingFromIdentity(identity: HandshakeIdentity): HandshakeTiming {
  const accept_ready_gap_ms =
    identity.accept_ready_gap_ms > 0
      ? identity.accept_ready_gap_ms
      : HANDSHAKE_DEFAULTS.accept_ready_gap_ms;
  const accept_reply_delay_ms =
    identity.accept_reply_delay_ms > 0
      ? identity.accept_reply_delay_ms
      : accept_ready_gap_ms * 2;
  const accept_timeout_sec =
    identity.accept_timeout_sec > 0
      ? identity.accept_timeout_sec
      : HANDSHAKE_DEFAULTS.accept_timeout_sec;
  const ready_timeout_sec =
    identity.ready_timeout_sec > 0
      ? identity.ready_timeout_sec
      : HANDSHAKE_DEFAULTS.ready_timeout_sec;
  const session_header_timeout_ms =
    identity.session_header_timeout_ms > 0
      ? identity.session_header_timeout_ms
      : HANDSHAKE_DEFAULTS.session_header_timeout_ms;
  const payload_header_timeout_ms =
    identity.payload_header_timeout_ms > 0
      ? identity.payload_header_timeout_ms
      : HANDSHAKE_DEFAULTS.payload_header_timeout_ms;
  const accept_dialog_sec = Math.max(
    1,
    accept_timeout_sec - HANDSHAKE_DEFAULTS.accept_receiver_margin_sec,
  );

  return {
    accept_ready_gap_ms,
    accept_reply_delay_ms,
    accept_timeout_sec,
    accept_dialog_sec,
    ready_timeout_sec,
    session_header_timeout_ms,
    handshake_poll_timeout_ms: HANDSHAKE_DEFAULTS.handshake_poll_timeout_ms,
    payload_header_timeout_ms,
  };
}
