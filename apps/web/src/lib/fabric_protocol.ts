export const VENDOR_ID = 0x1772;
export const PRODUCT_ID = 0x0006;
export const INTERFACE_NUMBER = 0;
export const EP_OUT = 2;
export const EP_IN = 1;
export const HEADER_SIZE = 32;
export const CHUNK_SIZE = 4 * 1024 * 1024;
export const HEADER_MAGIC = 'ROCKETBX';

export function buildHeader(fileSize: number, filename = ''): ArrayBuffer {
  const buf = new ArrayBuffer(HEADER_SIZE);
  const view = new DataView(buf);
  for (let i = 0; i < HEADER_MAGIC.length; i += 1) {
    view.setUint8(i, HEADER_MAGIC.charCodeAt(i));
  }
  view.setBigUint64(8, BigInt(fileSize), true);
  const nameBytes = new TextEncoder().encode(filename.slice(0, 15));
  for (let i = 0; i < nameBytes.length; i += 1) {
    view.setUint8(16 + i, nameBytes[i]!);
  }
  return buf;
}

export function parseHeader(buffer: ArrayBuffer | Uint8Array): { fileSize: number; filename: string } {
  const view =
    buffer instanceof Uint8Array
      ? new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength)
      : new DataView(buffer);
  if (view.byteLength < HEADER_SIZE) {
    throw new Error(`Header too short (${view.byteLength} bytes)`);
  }
  let magic = '';
  for (let i = 0; i < 8; i += 1) {
    magic += String.fromCharCode(view.getUint8(i));
  }
  if (magic !== HEADER_MAGIC) {
    throw new Error(`Bad header magic "${magic}"`);
  }
  let filename = '';
  for (let i = 16; i < HEADER_SIZE; i += 1) {
    const code = view.getUint8(i);
    if (code === 0) {
      break;
    }
    filename += String.fromCharCode(code);
  }
  return { fileSize: Number(view.getBigUint64(8, true)), filename };
}

export function concatChunks(chunks: Uint8Array[], totalBytes: number): Uint8Array {
  const out = new Uint8Array(totalBytes);
  let offset = 0;
  for (const chunk of chunks) {
    const take = Math.min(chunk.length, totalBytes - offset);
    out.set(chunk.subarray(0, take), offset);
    offset += take;
    if (offset >= totalBytes) {
      break;
    }
  }
  return out;
}

export function boothDisplayMibSWithJitter(base: number, jitterPct: number, roll01: number): number {
  if (base <= 0) {
    return 0;
  }
  if (jitterPct <= 0) {
    return base;
  }
  const clamped = Math.min(1, Math.max(0, roll01));
  const span = jitterPct / 100;
  return base * (1 + (clamped * 2 - 1) * span);
}

export function rollBoothDisplayMibS(base: number, jitterPct: number): number {
  return boothDisplayMibSWithJitter(base, jitterPct, Math.random());
}
