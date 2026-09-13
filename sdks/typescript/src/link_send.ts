import { boothLog } from './debug_log';
import { CHUNK_SIZE, buildHeader } from './protocol';
import { FabricUsbError } from './errors';
import { MAX_SESSION_FILE_BYTES } from './limits';
import { PAYLOAD_CHUNK_TIMEOUT_MS, SESSION_SEND_TIMEOUT_MS } from './link_types';
import { beginOutboundTransfer, endOutboundTransfer, prepareOutboundBus } from './link_bus';
import { enqueueControl, runUsb } from './link_queue';
import { transferOutWithRetry, transferOutWithTimeout } from './usb_xfer_out';
import { serializeSessionMessage } from './session_codec';
import type { FabricSessionMessage } from './session_types';
import type { FabricLink } from './fabric_link';

export async function sendSessionMessage(
  link: FabricLink,
  message: FabricSessionMessage,
): Promise<void> {
  const bytes = serializeSessionMessage(message);
  if (bytes.length > MAX_SESSION_FILE_BYTES) {
    throw new FabricUsbError('Session message too large');
  }
  const run = link.sessionSendTail.then(
    () => runSessionSend(link, message, bytes),
    () => runSessionSend(link, message, bytes),
  );
  link.sessionSendTail = run.then(
    () => undefined,
    () => undefined,
  );
  return run;
}

async function runSessionSend(
  link: FabricLink,
  message: FabricSessionMessage,
  bytes: Uint8Array,
): Promise<void> {
  await enqueueControl(link, 'sendSession', async () => {
    const device = link.getDevice();
    if (!device) {
      throw new FabricUsbError('USB not connected');
    }
    const recover = () => link.abortStuckTransferWithTimeout();
    beginOutboundTransfer(link);
    try {
      await prepareOutboundBus(link);
      await transferOutWithRetry(
        device,
        buildHeader(bytes.length, { frameKind: 'session' }),
        SESSION_SEND_TIMEOUT_MS,
        recover,
      );
      await transferOutWithRetry(device, new Uint8Array(bytes), SESSION_SEND_TIMEOUT_MS, recover);
      boothLog(link.fabricLeg, 'session_sent', message.kind);
    } finally {
      endOutboundTransfer(link);
    }
  });
}

export async function sendPayload(
  link: FabricLink,
  payload: Uint8Array,
  onProgress?: (done: number, total: number) => void,
  filename = '',
): Promise<void> {
  await runUsb(link, 1, 'sendPayload', async () => {
    const device = link.getDevice();
    if (!device) {
      throw new FabricUsbError('USB not connected');
    }
    beginOutboundTransfer(link);
    try {
      await prepareOutboundBus(link);
      await transferOutWithTimeout(
        device,
        buildHeader(payload.length, { frameKind: 'payload', filename }),
        PAYLOAD_CHUNK_TIMEOUT_MS,
      );
      onProgress?.(0, payload.length);
      let offset = 0;
      while (offset < payload.length) {
        const chunk = new Uint8Array(CHUNK_SIZE);
        const n = Math.min(payload.length - offset, CHUNK_SIZE);
        chunk.set(payload.subarray(offset, offset + n));
        await transferOutWithTimeout(device, chunk, PAYLOAD_CHUNK_TIMEOUT_MS);
        offset += n;
        onProgress?.(offset, payload.length);
      }
      boothLog(link.fabricLeg, 'payload_sent', String(payload.length));
    } finally {
      endOutboundTransfer(link);
    }
  });
}
