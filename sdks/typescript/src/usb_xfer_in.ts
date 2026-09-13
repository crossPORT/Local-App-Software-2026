import { EP_IN } from './protocol';
import { FabricUsbError } from './errors';
import { IN_FLIGHT_DRAIN_MS, IN_SETTLE_MS } from './link_types';
import { isBenignUsbListenError, mergeBuffers, sleep } from './usb_util';

async function drainInFlightTransfer(inFlight: Promise<USBInTransferResult>): Promise<void> {
  await Promise.race([inFlight.catch(() => undefined), sleep(IN_FLIGHT_DRAIN_MS)]);
  await sleep(IN_SETTLE_MS);
}

export async function transferInWithTimeout(
  device: USBDevice,
  byteCount: number,
  timeoutMs: number,
  cancelStuck?: () => Promise<void>,
): Promise<USBInTransferResult | null> {
  const inFlight = device.transferIn(EP_IN, byteCount);
  inFlight.catch(() => undefined);
  let timeoutId: number | undefined;
  try {
    const raced = await Promise.race([
      inFlight,
      new Promise<'timeout'>((resolve) => {
        timeoutId = window.setTimeout(() => resolve('timeout'), timeoutMs);
      }),
    ]);
    if (timeoutId !== undefined) {
      window.clearTimeout(timeoutId);
    }
    if (raced === 'timeout') {
      if (cancelStuck) {
        await cancelStuck();
      } else {
        await drainInFlightTransfer(inFlight);
      }
      return null;
    }
    return raced;
  } catch (err) {
    if (timeoutId !== undefined) {
      window.clearTimeout(timeoutId);
    }
    await drainInFlightTransfer(inFlight);
    if (isBenignUsbListenError(err)) {
      return null;
    }
    throw err;
  }
}

export async function transferInExact(device: USBDevice, byteCount: number): Promise<Uint8Array> {
  const parts: Uint8Array[] = [];
  let received = 0;
  while (received < byteCount) {
    const result = await device.transferIn(EP_IN, byteCount - received);
    if (result.status !== 'ok') {
      throw new FabricUsbError(`transferIn failed: ${result.status}`);
    }
    const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
    if (chunk.length === 0) {
      throw new FabricUsbError('transferIn returned no data');
    }
    parts.push(chunk);
    received += chunk.length;
  }
  return mergeBuffers(parts, byteCount);
}
