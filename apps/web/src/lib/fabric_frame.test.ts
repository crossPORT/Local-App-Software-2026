import { describe, expect, it } from 'vitest';
import {
  FRAME_KIND_PAYLOAD,
  FRAME_KIND_SESSION,
  frameKindToWire,
  wireToFrameKind,
} from './fabric_frame';

describe('fabric_frame', () => {
  it('maps wire kinds to session and payload', () => {
    expect(frameKindToWire('session')).toBe(FRAME_KIND_SESSION);
    expect(frameKindToWire('payload')).toBe(FRAME_KIND_PAYLOAD);
    expect(wireToFrameKind(FRAME_KIND_SESSION)).toBe('session');
    expect(wireToFrameKind(FRAME_KIND_PAYLOAD)).toBe('payload');
  });

  it('rejects invalid wire kinds', () => {
    expect(() => wireToFrameKind(0)).toThrow(/Invalid frame kind/);
    expect(() => wireToFrameKind(99)).toThrow(/Invalid frame kind/);
  });
});
