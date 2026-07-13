/** ROCKETBX header byte 16 — required on every frame (no legacy kind). */
export const FRAME_KIND_OFFSET = 16;
export const FRAME_FILENAME_OFFSET = 17;
export const FRAME_FILENAME_MAX = 15;

export const FRAME_KIND_SESSION = 1;
export const FRAME_KIND_PAYLOAD = 2;

export type WireFrameKind = 'session' | 'payload';

export function frameKindToWire(kind: WireFrameKind): number {
  return kind === 'session' ? FRAME_KIND_SESSION : FRAME_KIND_PAYLOAD;
}

export function wireToFrameKind(byte: number): WireFrameKind {
  if (byte === FRAME_KIND_SESSION) {
    return 'session';
  }
  if (byte === FRAME_KIND_PAYLOAD) {
    return 'payload';
  }
  throw new Error(`Invalid frame kind ${byte} (expected ${FRAME_KIND_SESSION}=session or ${FRAME_KIND_PAYLOAD}=payload)`);
}
