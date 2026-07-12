import {
  clearSavedUsbPairing,
  findPairedDevice,
  hasSavedSerial,
  isFabricDevice,
} from './usb_pairing';
import { INTERFACE_NUMBER } from './usb_ids';

export async function resolvePairedUsbDevice(): Promise<USBDevice> {
  if (!navigator.usb) {
    throw new Error('WebUSB unavailable');
  }
  if (!hasSavedSerial()) {
    throw new Error('No saved cable for this window — click Connect USB');
  }
  const devices = (await navigator.usb.getDevices()).filter(isFabricDevice);
  const device = findPairedDevice(devices);
  if (!device) {
    clearSavedUsbPairing();
    throw new Error(
      'Previously paired device not found — tap Connect USB and pick your cable',
    );
  }
  return device;
}

export async function forgetUsbDevice(device: USBDevice | null): Promise<void> {
  if (!navigator.usb) {
    clearSavedUsbPairing();
    return;
  }
  try {
    if (device) {
      if (device.opened) {
        try {
          await device.releaseInterface(INTERFACE_NUMBER);
        } catch {
          /* best effort */
        }
        await device.close();
      }
      await device.forget();
    }
  } catch {
    /* best effort */
  }
  clearSavedUsbPairing();
}
