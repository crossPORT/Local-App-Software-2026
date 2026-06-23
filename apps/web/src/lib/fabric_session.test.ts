import { describe, expect, it } from 'vitest';
import { readFixture } from '../../test/fixtures';
import {
  buildAnnounceMessage,
  buildSessionReply,
  parseSessionPayload,
  serializeSessionMessage,
  sessionKindFromString,
  sessionKindToString,
} from './fabric_session';
import type { IdentityProfile } from './types';

function sampleIdentity(): IdentityProfile {
  return {
    display_name: 'Alice',
    team: 'CAD',
    role: '',
    receive_status: 'open',
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
    booth_display_enabled: false,
    peers: [],
    config_path: 'test',
  };
}

describe('session message codec', () => {
  it('parses golden offer fixture', () => {
    const message = parseSessionPayload(readFixture('session/offer.sample.msg'));
    expect(message).not.toBeNull();
    expect(message!.kind).toBe('offer');
    expect(message!.from_name).toBe('Bob');
    expect(message!.session_id).toBe('golden-offer-01');
    expect(message!.total_bytes).toBe(4096);
  });

  it('round-trips serialize and parse', () => {
    const original = parseSessionPayload(readFixture('session/offer.sample.msg'));
    expect(original).not.toBeNull();
    const bytes = serializeSessionMessage(original!);
    const roundTrip = parseSessionPayload(bytes);
    expect(roundTrip).toEqual(original);
  });

  it('rejects payloads without session header', () => {
    expect(parseSessionPayload(new TextEncoder().encode('not-a-session\n'))).toBeNull();
  });

  it('rejects payloads missing session_id', () => {
    const bad = new TextEncoder().encode('FABRIC-SESSION-v1\nkind=offer\nfrom=Bob\n');
    expect(parseSessionPayload(bad)).toBeNull();
  });

  it('round-trips session kinds', () => {
    for (const kind of ['offer', 'accept', 'decline', 'ready', 'announce'] as const) {
      expect(sessionKindFromString(sessionKindToString(kind))).toBe(kind);
    }
    expect(sessionKindFromString('garbage')).toBe('unknown');
  });
});

describe('session message builders', () => {
  it('builds announce with port/receive note', () => {
    const message = buildAnnounceMessage(sampleIdentity(), 1, 'abc123');
    expect(message.kind).toBe('announce');
    expect(message.from_name).toBe('Alice');
    expect(message.note).toBe('port=1;receive=open;instance=abc123');
    expect(message.session_id).toHaveLength(16);
  });

  it('builds accept reply targeting offer sender', () => {
    const offer = parseSessionPayload(readFixture('session/offer.sample.msg'))!;
    const accept = buildSessionReply(offer, 'accept', 'Alice', 'CAD');
    expect(accept.kind).toBe('accept');
    expect(accept.session_id).toBe(offer.session_id);
    expect(accept.to_name).toBe('Bob');
    expect(accept.from_name).toBe('Alice');
  });
});
