import { CHUNK_SIZE, EP_IN, concatChunks } from './protocol';
import { FabricUsbError } from './errors';
import type { FabricLink } from './fabric_link';

export async function receivePayloadBytes(
  link: FabricLink,
  fileSize: number,
  onProgress?: (done: number, total: number) => void,
): Promise<Uint8Array> {
  const device = link.getDevice();
  if (!device) {
    throw new FabricUsbError('USB not connected');
  }
  const parts: Uint8Array[] = [];
  let received = 0;
  while (received < fileSize) {
    const result = await device.transferIn(EP_IN, CHUNK_SIZE);
    if (result.status !== 'ok') {
      throw new FabricUsbError(`Payload transferIn failed: ${result.status}`);
    }
    const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
    const take = Math.min(chunk.length, fileSize - received);
    parts.push(chunk.subarray(0, take));
    received += take;
    onProgress?.(received, fileSize);
  }
  return concatChunks(parts, fileSize);
}
