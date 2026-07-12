import { describe, expect, it } from 'vitest';
import { buildSwitchPacket, SWITCH_PACKET_SIZE } from '../transports/usb_switch';
import {
  buildAnnounceMessage,
  buildAnnounceNote,
  receiveStatusFromAnnounceNote,
} from './announce_note';

describe('switch packet', () => {
  it('puts dest in low nibble of byte 0', () => {
    const pkt = buildSwitchPacket(2);
    expect(pkt.length).toBe(SWITCH_PACKET_SIZE);
    expect(pkt[0]).toBe(0x02);
    expect(pkt[1]).toBe(0);
  });

  it('masks to low nibble', () => {
    expect(buildSwitchPacket(0x12)[0]).toBe(0x02);
  });
});

describe('announce note', () => {
  it('includes display port', () => {
    expect(buildAnnounceNote(1, 'open', 'abc')).toContain('port=2');
    const msg = buildAnnounceMessage('Alice', 'Team', 0, 'ask_first', 'inst1');
    expect(msg.kind).toBe('announce');
    expect(msg.note).toContain('port=1');
  });

  it('parses receive policy without inventing open', () => {
    expect(receiveStatusFromAnnounceNote('port=1;receive=ask_first')).toBe('ask_first');
    expect(receiveStatusFromAnnounceNote('port=2;receive=open')).toBe('open');
    expect(receiveStatusFromAnnounceNote('port=3;receive=busy')).toBe('busy');
    expect(receiveStatusFromAnnounceNote('port=1')).toBe('ask_first');
    expect(receiveStatusFromAnnounceNote(undefined)).toBe('ask_first');
  });
});
