import { describe, expect, it } from 'vitest';
import { buildAnnounceNote, parseAnnounceNote, resolveRemoteFabricPort } from './announce_note';

describe('announce note', () => {
  it('builds and parses port and receive status', () => {
    const note = buildAnnounceNote(1, 'open');
    expect(note).toBe('port=1;receive=open');
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
    expect(parseAnnounceNote('port=0;receive=auto_accept', 0).receiveStatus).toBe('open');
    expect(parseAnnounceNote('port=0;receive=busy', 0).receiveStatus).toBe('busy');
    expect(parseAnnounceNote('port=0;receive=unknown', 0).receiveStatus).toBe('ask_first');
  });

  it('falls back to default port when note omits port', () => {
    expect(parseAnnounceNote('receive=open', 2)).toEqual({
      portIndex: 2,
      receiveStatus: 'open',
      instanceId: '',
    });
  });

  it('maps same announced port to the remote fabric slot', () => {
    expect(resolveRemoteFabricPort(0, 0)).toBe(1);
    expect(resolveRemoteFabricPort(1, 1)).toBe(0);
    expect(resolveRemoteFabricPort(0, 1)).toBe(1);
  });
});
