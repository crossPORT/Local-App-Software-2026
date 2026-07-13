import { describe, expect, it } from 'vitest';
import { readFixture } from '../../../apps/web/test/fixtures';
import {
  buildSessionReply,
  parseSessionPayload,
  serializeSessionMessage,
  sessionKindFromString,
  sessionKindToString,
} from './session_codec';

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
  it('builds accept reply targeting offer sender', () => {
    const offer = parseSessionPayload(readFixture('session/offer.sample.msg'))!;
    const accept = buildSessionReply(offer, 'accept', 'Alice', 'CAD');
    expect(accept.kind).toBe('accept');
    expect(accept.session_id).toBe(offer.session_id);
    expect(accept.to_name).toBe('Bob');
    expect(accept.from_name).toBe('Alice');
  });
});
