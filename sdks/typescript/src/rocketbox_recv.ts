import type { ParsedHeader } from './protocol';
import type { SessionMessage } from './session_types';
import type { CircuitDataBuffer } from './rocketbox_buffer';

export async function recvFileTransfer(
  buf: CircuitDataBuffer,
  headerTimeoutMs: number,
  expectedBytes?: number,
  onProgress?: (done: number, total: number) => void,
): Promise<{ data: Uint8Array; filename: string }> {
  const header = await buf.waitHeader(headerTimeoutMs);
  if (header.frameKind !== 'payload') {
    throw new Error(`Unexpected frame kind: ${header.frameKind}`);
  }
  if (expectedBytes != null && header.fileSize !== expectedBytes) {
    throw new Error(`Size mismatch: expected ${expectedBytes}, got ${header.fileSize}`);
  }
  const data = await buf.waitPayload(header.fileSize, onProgress);
  return { data, filename: header.filename || 'payload.bin' };
}

export function recvHeader(buf: CircuitDataBuffer): Promise<ParsedHeader> {
  return buf.waitHeader(15_000);
}

export function recvPayload(
  buf: CircuitDataBuffer,
  n: number,
  onProgress?: (d: number, t: number) => void,
): Promise<Uint8Array> {
  return buf.waitPayload(n, onProgress);
}

export async function recvBytes(
  buf: CircuitDataBuffer,
  onProgress?: (d: number, t: number) => void,
): Promise<Uint8Array> {
  const header = await buf.waitHeader(15_000);
  return buf.waitPayload(header.fileSize, onProgress);
}

export async function tryRecvSession(_ms: number): Promise<SessionMessage | null> {
  return null;
}
