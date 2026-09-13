import { EP_OUT } from './protocol';
import { FabricUsbError } from './errors';
import { isUsbTransferContentionError, sleep } from './usb_util';

async function transferOutChecked(device: USBDevice, data: BufferSource): Promise<void> {
  const result = await device.transferOut(EP_OUT, data);
  if (result.status !== 'ok') {
    throw new FabricUsbError(`transferOut failed: ${result.status}`);
  }
}

export async function transferOutWithTimeout(
  device: USBDevice,
  data: BufferSource,
  timeoutMs: number,
): Promise<void> {
  const inFlight = transferOutChecked(device, data);
  inFlight.catch(() => undefined);
  let timeoutId: number | undefined;
  const timeout = new Promise<'timeout'>((resolve) => {
    timeoutId = window.setTimeout(() => resolve('timeout'), timeoutMs);
  });
  try {
    const raced = await Promise.race([inFlight.then(() => 'ok' as const), timeout]);
    if (raced === 'timeout') {
      throw new FabricUsbError(`transferOut timed out after ${timeoutMs}ms`);
    }
  } finally {
    if (timeoutId !== undefined) {
      window.clearTimeout(timeoutId);
    }
  }
}

export async function transferOutWithRetry(
  device: USBDevice,
  data: BufferSource,
  timeoutMs: number,
  recover: () => Promise<void>,
): Promise<void> {
  for (let attempt = 0; attempt < 2; attempt += 1) {
    try {
      await transferOutWithTimeout(device, data, timeoutMs);
      return;
    } catch (err) {
      await recover();
      const contention = isUsbTransferContentionError(err);
      if (!contention || attempt === 1) {
        throw err;
      }
      await sleep(60);
    }
  }
}
