import { boothLog } from './debug_log';
import { CHUNK_SIZE, EP_IN, HEADER_SIZE, concatChunks, parseHeader } from './protocol';
import { MAX_SESSION_FILE_BYTES } from './limits';
import { SESSION_BODY_IN_TIMEOUT_MS } from './link_types';
import { isBenignUsbListenError, sleep } from './usb_util';
import { transferInWithTimeout } from './usb_xfer_in';
import { parseSessionPayload } from './session_codec';
import type { FabricSessionMessage } from './session_types';
import type { FabricLink } from './fabric_link';
import { receivePayloadBytes } from './link_payload';
import { runUsb } from './link_queue';

function headerRead(link: FabricLink, device: USBDevice): Promise<USBInTransferResult | null> {
  if (link.pendingHeaderRead) {
    return link.pendingHeaderRead;
  }
  const read = device.transferIn(EP_IN, HEADER_SIZE).then(
    (result) => result as USBInTransferResult | null,
    (err) => {
      if (!isBenignUsbListenError(err) && (err as Error)?.name !== 'AbortError') {
        boothLog(link.fabricLeg, 'usb_recv_fail', (err as Error).message);
      }
      return null;
    },
  );
  link.pendingHeaderRead = read;
  return read;
}

async function receivePayloadBytesWithTimeout(
  link: FabricLink,
  fileSize: number,
  chunkTimeoutMs: number,
): Promise<Uint8Array | null> {
  const device = link.getDevice();
  if (!device) {
    return null;
  }
  const parts: Uint8Array[] = [];
  let received = 0;
  while (received < fileSize) {
    const result = await transferInWithTimeout(device, CHUNK_SIZE, chunkTimeoutMs, () =>
      link.abortStuckTransfer(),
    );
    if (!result || result.status !== 'ok') {
      boothLog(link.fabricLeg, 'session_body_timeout', `${received}/${fileSize}`);
      return null;
    }
    const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
    if (chunk.length === 0) {
      return null;
    }
    const take = Math.min(chunk.length, fileSize - received);
    parts.push(chunk.subarray(0, take));
    received += take;
  }
  return concatChunks(parts, fileSize);
}

export async function tryReceiveSessionOnce(
  link: FabricLink,
  headerTimeoutMs: number,
): Promise<FabricSessionMessage | null> {
  const device = link.getDevice();
  if (!device) {
    return null;
  }
  try {
    const pending = headerRead(link, device);
    const raced = await Promise.race([
      pending.then((result) => ({ ready: true as const, result })),
      sleep(headerTimeoutMs).then(() => ({ ready: false as const, result: null })),
    ]);
    if (!raced.ready) {
      return null;
    }
    link.clearPendingHeaderRead();
    const result = raced.result;
    if (!result || result.status !== 'ok' || !result.data?.byteLength) {
      return null;
    }
    const chunk = new Uint8Array(result.data.buffer, result.data.byteOffset, result.data.byteLength);
    if (chunk.length < HEADER_SIZE) {
      return null;
    }
    const header = parseHeader(chunk.subarray(0, HEADER_SIZE));
    if (header.frameKind !== 'session') {
      boothLog(link.fabricLeg, 'frame_kind_reject', `expected session got ${header.frameKind}`);
      if (header.fileSize > 0) {
        await receivePayloadBytes(link, header.fileSize).catch(() => {});
      }
      return null;
    }
    if (header.fileSize === 0 || header.fileSize > MAX_SESSION_FILE_BYTES) {
      if (header.fileSize > MAX_SESSION_FILE_BYTES) {
        await receivePayloadBytes(link, header.fileSize).catch(() => {});
      }
      return null;
    }
    const data = await receivePayloadBytesWithTimeout(link, header.fileSize, SESSION_BODY_IN_TIMEOUT_MS);
    if (!data) {
      return null;
    }
    return parseSessionPayload(data);
  } catch (err) {
    link.clearPendingHeaderRead();
    if (isBenignUsbListenError(err)) {
      return null;
    }
    boothLog(link.fabricLeg, 'usb_recv_fail', (err as Error).message);
    return null;
  }
}

export function tryReceiveSessionMessage(
  link: FabricLink,
  headerTimeoutMs: number,
): Promise<FabricSessionMessage | null> {
  return runUsb(link, 0, 'tryReceiveSession', () => tryReceiveSessionOnce(link, headerTimeoutMs));
}

export { receiveFileTransfer } from './link_recv_file';
