import { INTERFACE_NUMBER } from './protocol';
import { FabricUsbError } from './errors';
import { clearEndpointHalts } from './usb_claim';
import { sleep } from './usb_util';

const USB_OPEN_ATTEMPTS = 3;

export async function resetDeviceHandle(device: USBDevice): Promise<void> {
  if (!device.opened) {
    return;
  }
  try {
    await device.releaseInterface(INTERFACE_NUMBER);
  } catch {
    /* best effort */
  }
  try {
    await device.close();
  } catch {
    /* best effort */
  }
}

export function mapUsbOpenError(err: unknown): FabricUsbError {
  const message = (err as Error)?.message ?? String(err);
  if (message.includes('disconnected')) {
    return new FabricUsbError(
      'Could not reach the USB device — click Reconnect USB. If it keeps failing, power-cycle the RocketBox.',
    );
  }
  if (message.includes('Access denied')) {
    return new FabricUsbError(
      'USB access denied — close other browser tabs using this cable, then click Connect USB.',
    );
  }
  return new FabricUsbError(`Could not open USB device — ${message}`);
}

async function openFabricDevice(device: USBDevice): Promise<void> {
  if (!device.opened) {
    try {
      await device.open();
    } catch (err) {
      throw mapUsbOpenError(err);
    }
  }
  if (device.configuration == null) {
    await device.selectConfiguration(1);
  }
  try {
    await device.claimInterface(INTERFACE_NUMBER);
  } catch (err) {
    const message = (err as Error).message ?? '';
    try {
      await device.releaseInterface(INTERFACE_NUMBER);
      await device.claimInterface(INTERFACE_NUMBER);
    } catch {
      if (message.includes('claim') || message.includes('busy') || message.includes('Access')) {
        throw new FabricUsbError(
          'USB interface is busy — close other RocketBox App tabs or native apps using this cable, then try Forget USB device.',
        );
      }
      throw new FabricUsbError(`Could not claim USB interface — ${message}`);
    }
  }
  await clearEndpointHalts(device);
}

export async function openFabricDeviceWithRetry(
  device: USBDevice,
  attempts = USB_OPEN_ATTEMPTS,
): Promise<void> {
  let lastErr: unknown;
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    if (attempt > 0) {
      await sleep(400 * attempt);
      await resetDeviceHandle(device);
    }
    try {
      await openFabricDevice(device);
      return;
    } catch (err) {
      lastErr = err;
    }
  }
  throw lastErr instanceof FabricUsbError ? lastErr : mapUsbOpenError(lastErr);
}
