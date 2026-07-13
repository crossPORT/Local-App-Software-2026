import { PRODUCT_ID, VENDOR_ID } from './usb_ids';

export const SELECTED_SERIAL_KEY = 'rocketbox_usb_serial';

function legacySerialKey(leg: number): string {
  return `rocketbox_serial_port${leg}`;
}

export function getSavedSerial(): string | null {
  const primary = sessionStorage.getItem(SELECTED_SERIAL_KEY);
  if (primary) {
    return primary;
  }
  for (let leg = 0; leg < 4; leg += 1) {
    const legacy = sessionStorage.getItem(legacySerialKey(leg));
    if (legacy) {
      sessionStorage.setItem(SELECTED_SERIAL_KEY, legacy);
      return legacy;
    }
  }
  return null;
}

export function clearSavedSerial(): void {
  sessionStorage.removeItem(SELECTED_SERIAL_KEY);
  for (let leg = 0; leg < 4; leg += 1) {
    sessionStorage.removeItem(legacySerialKey(leg));
  }
}

export function rememberSerial(device: USBDevice): void {
  if (device.serialNumber) {
    sessionStorage.setItem(SELECTED_SERIAL_KEY, device.serialNumber);
  }
}

export function hasSavedSerial(): boolean {
  return getSavedSerial() != null;
}

export function clearSavedUsbPairing(): void {
  clearSavedSerial();
}

export function isFabricDevice(device: USBDevice): boolean {
  return device.vendorId === VENDOR_ID && device.productId === PRODUCT_ID;
}

export function sortDevicesBySerial<T extends { serialNumber?: string }>(
  devices: T[],
): T[] {
  return [...devices].sort((a, b) => (a.serialNumber ?? '').localeCompare(b.serialNumber ?? ''));
}

export function findPairedDevice(devices: USBDevice[]): USBDevice | null {
  const sorted = sortDevicesBySerial(devices);
  if (sorted.length === 0) {
    return null;
  }
  const savedSerial = getSavedSerial();
  if (savedSerial) {
    const match = sorted.find((d) => d.serialNumber === savedSerial);
    if (match) {
      return match;
    }
  }
  if (sorted.length === 1) {
    return sorted[0];
  }
  return null;
}

export async function countDevices(): Promise<number> {
  if (!navigator.usb) {
    return 0;
  }
  return (await navigator.usb.getDevices()).filter(isFabricDevice).length;
}
