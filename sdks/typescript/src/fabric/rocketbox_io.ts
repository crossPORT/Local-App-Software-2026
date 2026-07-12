import type { Connection } from '../connection';
import {
  CHUNK_SIZE,
  HEADER_SIZE,
  buildHeader,
  parseHeader,
  type ParsedHeader,
} from './protocol';
import {
  parseSessionPayload,
  serializeSessionMessage,
} from './session_codec';
import type { FabricSessionMessage } from './session_types';

export function concatBytes(a: Uint8Array, b: Uint8Array): Uint8Array {
  const out = new Uint8Array(a.length + b.length);
  out.set(a, 0);
  out.set(b, a.length);
  return out;
}

export function isLengthPrefixedFrame(bytes: Uint8Array): boolean {
  if (bytes.length < 4) return false;
  const len = new DataView(bytes.buffer, bytes.byteOffset, 4).getUint32(0);
  return len === bytes.length - 4;
}

/** Let React paint liveMbps / activity bars between USB chunks. */
function yieldToUi(): Promise<void> {
  return new Promise((resolve) => {
    if (typeof requestAnimationFrame === 'function') {
      requestAnimationFrame(() => resolve());
      return;
    }
    setTimeout(resolve, 0);
  });
}

export async function sendSessionOnCircuit(
  connection: Connection,
  message: FabricSessionMessage,
): Promise<void> {
  await connection.sendMessage(serializeSessionMessage(message));
}

export async function sendFileOnCircuit(
  connection: Connection,
  payload: Uint8Array,
  filename: string,
  onProgress?: (done: number, total: number) => void,
): Promise<void> {
  const header = new Uint8Array(buildHeader(payload.length, { frameKind: 'payload', filename }));
  await connection.send(header);
  onProgress?.(0, payload.length);
  await yieldToUi();

  let offset = 0;
  while (offset < payload.length) {
    const n = Math.min(CHUNK_SIZE, payload.length - offset);
    await connection.send(payload.subarray(offset, offset + n));
    offset += n;
    onProgress?.(offset, payload.length);
    await yieldToUi();
  }
}

export function parseIncomingSession(bytes: Uint8Array): FabricSessionMessage | null {
  return parseSessionPayload(bytes);
}

export function waitUntil<T>(
  tryGet: () => T | null,
  timeoutMs: number,
  err: string,
): Promise<T> {
  const start = Date.now();
  return new Promise((resolve, reject) => {
    const tick = () => {
      try {
        const v = tryGet();
        if (v != null) {
          resolve(v);
          return;
        }
      } catch (e) {
        reject(e);
        return;
      }
      if (Date.now() - start > timeoutMs) {
        reject(new Error(err));
        return;
      }
      window.setTimeout(tick, 20);
    };
    tick();
  });
}

export function takeHeader(buf: { current: Uint8Array }): ParsedHeader | null {
  if (buf.current.length < HEADER_SIZE) return null;
  const header = parseHeader(buf.current.subarray(0, HEADER_SIZE));
  buf.current = buf.current.subarray(HEADER_SIZE);
  return header;
}

export function takeBytes(
  buf: { current: Uint8Array },
  size: number,
  onProgress?: (done: number, total: number) => void,
): Uint8Array | null {
  onProgress?.(Math.min(buf.current.length, size), size);
  if (buf.current.length < size) return null;
  const out = buf.current.subarray(0, size).slice();
  buf.current = buf.current.subarray(size);
  return out;
}
