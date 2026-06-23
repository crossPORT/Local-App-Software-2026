import { describe, expect, it } from 'vitest';
import {
  effectiveDisplayMbps,
  formatBytes,
  formatMbps,
  formatTransferDoneMessage,
  formatTransferLiveMessage,
  isOutboundHandshakeWait,
  isTransferCompleteMessage,
  receiveStatusLabel,
} from './format';

describe('formatBytes', () => {
  it('formats byte scales', () => {
    expect(formatBytes(512)).toBe('512 B');
    expect(formatBytes(2048)).toBe('2.0 KiB');
    expect(formatBytes(5 * 1024 * 1024)).toBe('5.00 MiB');
    expect(formatBytes(3 * 1024 * 1024 * 1024)).toBe('3.00 GiB');
  });
});

describe('formatMbps', () => {
  it('formats throughput labels', () => {
    expect(formatMbps(0)).toBe('—');
    expect(formatMbps(0.5)).toMatch(/KiB\/s/);
    expect(formatMbps(12.3)).toBe('12.3 MiB/s');
    expect(formatMbps(2048)).toMatch(/GiB\/s/);
  });
});

describe('transfer status messages', () => {
  it('detects completion messages', () => {
    expect(isTransferCompleteMessage('Sent at 12.3 MiB/s')).toBe(true);
    expect(isTransferCompleteMessage('Received')).toBe(true);
    expect(isTransferCompleteMessage('Sending at 1.0 MiB/s')).toBe(false);
  });

  it('formats live and done messages', () => {
    expect(formatTransferLiveMessage(true, 0)).toBe('Sending…');
    expect(formatTransferDoneMessage(false, 10)).toBe('Received at 10.0 MiB/s');
  });

  it('detects outbound handshake wait copy', () => {
    expect(isOutboundHandshakeWait('Waiting for Bob to accept…')).toBe(true);
    expect(isOutboundHandshakeWait('Sending at 1 MiB/s')).toBe(false);
  });
});

describe('effectiveDisplayMbps', () => {
  it('prefers measured live rate', () => {
    expect(effectiveDisplayMbps(100, 7168, true)).toBe(100);
  });

  it('uses booth display rate only while busy with payload progress', () => {
    expect(effectiveDisplayMbps(0, 7168, true, 1024)).toBe(7168);
    expect(effectiveDisplayMbps(0, 7168, true, 0)).toBe(0);
    expect(effectiveDisplayMbps(0, 7168, false)).toBe(0);
  });
});

describe('receiveStatusLabel', () => {
  it('maps receive policy to user copy', () => {
    expect(receiveStatusLabel('open')).toContain('automatically');
    expect(receiveStatusLabel('busy')).toContain('Not accepting');
    expect(receiveStatusLabel('ask_first')).toContain('Asks');
  });
});
