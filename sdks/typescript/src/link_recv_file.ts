import { boothLog } from './debug_log';
import { HEADER_SIZE, parseHeader } from './protocol';
import { FabricUsbError } from './errors';
import { parseSessionPayload } from './session_codec';
import { transferInExact } from './usb_xfer_in';
import { receivePayloadBytes } from './link_payload';
import { runUsb } from './link_queue';
import type { FabricLink } from './fabric_link';

export async function receiveFileTransfer(
  link: FabricLink,
  headerTimeoutMs: number,
  expectedBytes = 0,
  onProgress?: (done: number, total: number) => void,
): Promise<{ data: Uint8Array; filename: string }> {
  return runUsb(link, 1, 'receiveFileTransfer', async () => {
    const device = link.getDevice();
    if (!device) {
      throw new FabricUsbError('USB not connected');
    }
    const deadline = Date.now() + headerTimeoutMs;
    let skipped = 0;
    const maxSkips = 16;
    while (Date.now() < deadline && skipped < maxSkips) {
      const remaining = Math.max(1, deadline - Date.now());
      let timeoutId: number | undefined;
      let timedOut = false;
      try {
        const headerBuf = await Promise.race([
          transferInExact(device, HEADER_SIZE),
          new Promise<never>((_, reject) => {
            timeoutId = window.setTimeout(() => {
              timedOut = true;
              reject(new FabricUsbError('payload header timeout'));
            }, remaining);
          }),
        ]);
        if (timeoutId !== undefined) {
          window.clearTimeout(timeoutId);
        }
        const header = parseHeader(headerBuf);
        if (header.frameKind === 'session') {
          const data = await receivePayloadBytes(link, header.fileSize);
          const parsed = parseSessionPayload(data);
          if (parsed) {
            boothLog(link.fabricLeg, 'stray_session_skipped', parsed.kind);
            link.emit({ type: 'session', message: parsed });
          }
          skipped += 1;
          continue;
        }
        if (header.frameKind !== 'payload') {
          throw new FabricUsbError('Invalid frame kind during payload receive');
        }
        if (header.fileSize === 0) {
          skipped += 1;
          continue;
        }
        if (expectedBytes > 0 && header.fileSize !== expectedBytes) {
          throw new FabricUsbError(
            `Expected ${expectedBytes} byte payload but header announced ${header.fileSize}`,
          );
        }
        const trackProgress = expectedBytes > 0 && header.fileSize === expectedBytes;
        const payload = await receivePayloadBytes(
          link,
          header.fileSize,
          trackProgress ? onProgress : undefined,
        );
        return { data: payload, filename: header.filename || 'download.bin' };
      } catch (err) {
        if (timeoutId !== undefined) {
          window.clearTimeout(timeoutId);
        }
        if (timedOut) {
          await link.abortStuckTransfer();
        }
        throw err;
      }
    }
    throw new FabricUsbError('payload header timeout');
  });
}
