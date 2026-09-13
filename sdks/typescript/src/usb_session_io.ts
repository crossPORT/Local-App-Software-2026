import { FabricUsbError } from './errors';
import type { ParsedHeader } from './protocol';
import type { FabricUsbSession } from './usb_session';

export async function sessionSendBytes(
  session: FabricUsbSession,
  payload: Uint8Array,
  onProgress?: (done: number, total: number) => void,
  filename = '',
): Promise<void> {
  await session.link.sendPayload(payload, onProgress, filename);
}

export async function sessionReceiveHeader(session: FabricUsbSession): Promise<ParsedHeader> {
  const message = await session.link.tryReceiveSessionMessage(2000);
  if (message) {
    throw new FabricUsbError('Expected payload header, got session message');
  }
  throw new FabricUsbError('receiveHeader not used — use receiveFileTransfer');
}

export async function sessionReceivePayload(
  session: FabricUsbSession,
  fileSize: number,
  onProgress?: (done: number, total: number) => void,
): Promise<Uint8Array> {
  const { data } = await session.link.receiveFileTransfer(15000, fileSize, onProgress);
  return data;
}

export async function sessionReceiveBytes(
  session: FabricUsbSession,
  onProgress?: (done: number, total: number) => void,
): Promise<Uint8Array> {
  const { data } = await session.link.receiveFileTransfer(15000, 0, onProgress);
  return data;
}
