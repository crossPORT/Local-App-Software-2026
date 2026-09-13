import { EP_IN, EP_OUT, INTERFACE_NUMBER } from './protocol';
import { FabricUsbError } from './errors';
import { sleep } from './usb_util';

export async function clearEndpointHalts(device: USBDevice): Promise<void> {
  if (typeof device.clearHalt !== 'function') {
    return;
  }
  try {
    await device.clearHalt('out', EP_OUT);
  } catch {
    /* best effort */
  }
  try {
    await device.clearHalt('in', EP_IN);
  } catch {
    /* best effort */
  }
}

export async function ensureInterfaceReady(device: USBDevice): Promise<void> {
  if (!device.opened) {
    throw new FabricUsbError('USB not connected');
  }
  if (device.configuration == null) {
    await device.selectConfiguration(1);
  }
  for (let attempt = 0; attempt < 5; attempt += 1) {
    try {
      await device.claimInterface(INTERFACE_NUMBER);
      await clearEndpointHalts(device);
      return;
    } catch (err) {
      const message = (err as Error).message ?? '';
      if (message.includes('operation that changes the device state is in progress')) {
        await sleep(120 * (attempt + 1));
        continue;
      }
      if (
        message.includes('already claimed') ||
        message.includes('Unable to claim') ||
        message.includes('not part of a claimed')
      ) {
        try {
          await device.releaseInterface(INTERFACE_NUMBER);
        } catch {
          /* best effort */
        }
        if (device.configuration == null) {
          await device.selectConfiguration(1);
        }
        await sleep(80 * (attempt + 1));
        continue;
      }
      throw err;
    }
  }
  throw new FabricUsbError('USB interface claim failed after retries');
}
