import { describe, expect, it } from 'vitest';
import {
  buildAnnounceNote,
  parseAnnounceNote,
  resolveRemoteFabricLeg,
  resolveRemoteFabricPort,
} from './announce_note';

describe('announce note', () => {
  it('builds and parses 1-based wire ports', () => {
    const note = buildAnnounceNote(1, 'open');
    expect(note).toBe('port=2;receive=open');
    expect(parseAnnounceNote(note, 0)).toEqual({
      portIndex: 1,
      receiveStatus: 'open',
      instanceId: '',
    });
  });

  it('includes instance id when provided', () => {
    const note = buildAnnounceNote(0, 'ask_first', 'abc123');
    expect(note).toContain('instance=abc123');
    expect(parseAnnounceNote(note, 1).instanceId).toBe('abc123');
  });

  it('normalizes receive aliases', () => {
    expect(parseAnnounceNote('port=1;receive=auto_accept', 0).receiveStatus).toBe('open');
    expect(parseAnnounceNote('port=1;receive=busy', 0).receiveStatus).toBe('busy');
    expect(parseAnnounceNote('port=1;receive=unknown', 0).receiveStatus).toBe('ask_first');
  });

  it('accepts 1–4 wire ports and legacy 0–3', () => {
    expect(parseAnnounceNote('port=4;receive=open', 0).portIndex).toBe(3);
    expect(parseAnnounceNote('port=3;receive=open', 0).portIndex).toBe(2);
    expect(parseAnnounceNote('port=2;receive=open', 0).portIndex).toBe(1);
    expect(parseAnnounceNote('port=0;receive=open', 0).portIndex).toBe(0);
    expect(parseAnnounceNote('port=9;receive=open', 0).portIndex).toBe(0);
  });

  it('falls back to default port when note omits port', () => {
    expect(parseAnnounceNote('receive=open', 2)).toEqual({
      portIndex: 2,
      receiveStatus: 'open',
      instanceId: '',
    });
  });

  it('rejects echo announces on the same leg', () => {
    expect(resolveRemoteFabricLeg(0, 0)).toBeNull();
    expect(resolveRemoteFabricLeg(2, 2)).toBeNull();
  });

  it('accepts remote legs on a four-leg fabric', () => {
    expect(resolveRemoteFabricLeg(0, 1)).toBe(1);
    expect(resolveRemoteFabricLeg(0, 3)).toBe(3);
    expect(resolveRemoteFabricLeg(2, 0)).toBe(0);
  });

  it('deprecated resolveRemoteFabricPort falls back for echo', () => {
    expect(resolveRemoteFabricPort(0, 0)).toBe(0);
    expect(resolveRemoteFabricPort(0, 2)).toBe(2);
  });
});
