import { describe, expect, it } from 'vitest';
import {
  HEADER_MAGIC,
  HEADER_SIZE,
  boothDisplayMibSWithJitter,
  buildHeader,
  concatChunks,
  parseHeader,
} from './fabric_protocol';

describe('ROCKETBX header', () => {
  it('builds and parses a payload header', () => {
    const header = buildHeader(4096, { frameKind: 'payload', filename: 'handshake.bin' });
    expect(header.byteLength).toBe(HEADER_SIZE);
    const parsed = parseHeader(header);
    expect(parsed.fileSize).toBe(4096);
    expect(parsed.filename).toBe('handshake.bin');
    expect(parsed.frameKind).toBe('payload');
  });

  it('builds and parses a session header', () => {
    const parsed = parseHeader(buildHeader(128, { frameKind: 'session' }));
    expect(parsed.fileSize).toBe(128);
    expect(parsed.frameKind).toBe('session');
    expect(parsed.filename).toBe('');
  });

  it('truncates filename to 15 bytes', () => {
    const parsed = parseHeader(
      buildHeader(1, { frameKind: 'payload', filename: 'very-long-filename.bin' }),
    );
    expect(parsed.filename).toBe('very-long-filen');
  });

  it('rejects bad magic', () => {
    const buf = new ArrayBuffer(HEADER_SIZE);
    expect(() => parseHeader(buf)).toThrow(/Bad header magic/);
  });

  it('rejects invalid frame kind', () => {
    const buf = buildHeader(1, { frameKind: 'payload' });
    const view = new DataView(buf);
    view.setUint8(16, 0);
    expect(() => parseHeader(buf)).toThrow(/Invalid frame kind/);
  });

  it('rejects short buffers', () => {
    expect(() => parseHeader(new Uint8Array(8))).toThrow(/Header too short/);
  });

  it('preserves magic bytes', () => {
    const view = new Uint8Array(buildHeader(0, { frameKind: 'session' }));
    const magic = String.fromCharCode(...view.slice(0, HEADER_MAGIC.length));
    expect(magic).toBe(HEADER_MAGIC);
  });
});

describe('concatChunks', () => {
  it('joins chunks up to totalBytes', () => {
    const out = concatChunks(
      [new Uint8Array([1, 2]), new Uint8Array([3, 4, 5])],
      4,
    );
    expect(Array.from(out)).toEqual([1, 2, 3, 4]);
  });

  it('returns empty array for zero total', () => {
    expect(concatChunks([new Uint8Array([1])], 0).length).toBe(0);
  });
});

describe('boothDisplayMibSWithJitter', () => {
  const base = 7168;
  const pct = 3;

  it('matches C++ jitter bounds at roll 0 and 1', () => {
    const low = boothDisplayMibSWithJitter(base, pct, 0);
    const high = boothDisplayMibSWithJitter(base, pct, 1);
    expect(low).toBeGreaterThan(6952);
    expect(low).toBeLessThan(6953);
    expect(high).toBeGreaterThan(7382);
    expect(high).toBeLessThan(7384);
    const mid = boothDisplayMibSWithJitter(base, pct, 0.5);
    expect(mid).toBeGreaterThan(7167);
    expect(mid).toBeLessThan(7169);
  });

  it('returns base when jitter percent is zero', () => {
    const value = boothDisplayMibSWithJitter(7168, 0, 0.42);
    expect(value).toBeGreaterThan(7167);
    expect(value).toBeLessThan(7169);
  });

  it('returns zero when base is zero', () => {
    expect(boothDisplayMibSWithJitter(0, pct, 0.5)).toBe(0);
  });
});
