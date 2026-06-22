import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { parseAnnounceNote } from './announce_note';
import { parseSessionPayload, sessionKindFromString, sessionKindToString } from './fabric_session';

const fixtureRoot = join(dirname(fileURLToPath(import.meta.url)), '../../../../tests/fixtures');

describe('fabric_session parity with C++ fixtures', () => {
  it('parses golden offer fixture', () => {
    const raw = readFileSync(join(fixtureRoot, 'session/offer.sample.msg'));
    const message = parseSessionPayload(new Uint8Array(raw));
    expect(message).not.toBeNull();
    expect(message!.kind).toBe('offer');
    expect(message!.from_name).toBe('Bob');
    expect(message!.team).toBe('Test');
    expect(message!.session_id).toBe('golden-offer-01');
    expect(message!.to_name).toBe('Alice');
    expect(message!.note).toBe('fixture');
    expect(message!.payload_type).toBe('file');
    expect(message!.payload_name).toBe('handshake.bin');
    expect(message!.file_count).toBe(1);
    expect(message!.total_bytes).toBe(4096);
  });

  it('round-trips session kinds', () => {
    const kinds = ['offer', 'accept', 'decline', 'ready', 'announce'] as const;
    for (const kind of kinds) {
      expect(sessionKindFromString(sessionKindToString(kind))).toBe(kind);
    }
    expect(sessionKindFromString('garbage')).toBe('unknown');
  });
});

describe('announce_note parity', () => {
  it('parses port and receive status', () => {
    const parsed = parseAnnounceNote('port=1;receive=open', 0);
    expect(parsed.portIndex).toBe(1);
    expect(parsed.receiveStatus).toBe('open');
  });
});
