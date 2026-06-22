import { describe, expect, it } from 'vitest';
import { HANDSHAKE_DEFAULTS, handshakeTimingFromIdentity } from './session_handshake';
import type { IdentityProfile } from './types';

function identity(overrides: Partial<IdentityProfile> = {}): IdentityProfile {
  return {
    display_name: 'Alice',
    team: 'CAD',
    role: '',
    receive_status: 'ask_first',
    receive_folder: '~/Incoming',
    transfer_timeout_ms: 0,
    usb_inflight_mb: 0,
    accept_ready_gap_ms: 0,
    accept_reply_delay_ms: 0,
    accept_timeout_sec: 0,
    ready_timeout_sec: 0,
    session_header_timeout_ms: 0,
    payload_header_timeout_ms: 0,
    booth_display_mib_s: 0,
    booth_display_jitter_pct: 0,
    booth_display_enabled: true,
    peers: [],
    config_path: 'test',
    ...overrides,
  };
}

describe('handshakeTimingFromIdentity', () => {
  it('uses defaults when identity tuning keys are zero', () => {
    const timing = handshakeTimingFromIdentity(identity());
    expect(timing.accept_ready_gap_ms).toBe(HANDSHAKE_DEFAULTS.accept_ready_gap_ms);
    expect(timing.accept_reply_delay_ms).toBe(HANDSHAKE_DEFAULTS.accept_ready_gap_ms * 2);
    expect(timing.accept_timeout_sec).toBe(HANDSHAKE_DEFAULTS.accept_timeout_sec);
    expect(timing.accept_dialog_sec).toBe(
      HANDSHAKE_DEFAULTS.accept_timeout_sec - HANDSHAKE_DEFAULTS.accept_receiver_margin_sec,
    );
    expect(timing.ready_timeout_sec).toBe(HANDSHAKE_DEFAULTS.ready_timeout_sec);
    expect(timing.session_header_timeout_ms).toBe(HANDSHAKE_DEFAULTS.session_header_timeout_ms);
    expect(timing.handshake_poll_timeout_ms).toBe(HANDSHAKE_DEFAULTS.handshake_poll_timeout_ms);
    expect(timing.payload_header_timeout_ms).toBe(HANDSHAKE_DEFAULTS.payload_header_timeout_ms);
  });

  it('honours explicit tuning from config', () => {
    const timing = handshakeTimingFromIdentity(
      identity({
        accept_ready_gap_ms: 200,
        accept_reply_delay_ms: 500,
        accept_timeout_sec: 30,
        ready_timeout_sec: 10,
        session_header_timeout_ms: 1500,
        payload_header_timeout_ms: 12000,
      }),
    );
    expect(timing.accept_ready_gap_ms).toBe(200);
    expect(timing.accept_reply_delay_ms).toBe(500);
    expect(timing.accept_timeout_sec).toBe(30);
    expect(timing.accept_dialog_sec).toBe(27);
    expect(timing.ready_timeout_sec).toBe(10);
    expect(timing.session_header_timeout_ms).toBe(1500);
    expect(timing.payload_header_timeout_ms).toBe(12000);
  });

  it('defaults accept_reply_delay_ms to double accept_ready_gap_ms', () => {
    const timing = handshakeTimingFromIdentity(identity({ accept_ready_gap_ms: 250 }));
    expect(timing.accept_ready_gap_ms).toBe(250);
    expect(timing.accept_reply_delay_ms).toBe(500);
  });
});
