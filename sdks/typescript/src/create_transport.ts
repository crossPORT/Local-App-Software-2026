import { FabricUsbSession } from './usb_session';
import type { RocketBoxTransport } from './transport';

/** USB-only factory (no TCP/sim plane). */
export function createRocketBoxUsbTransport(): RocketBoxTransport {
  return new FabricUsbSession();
}

export function createRocketBoxTransport(): RocketBoxTransport {
  return createRocketBoxUsbTransport();
}

export async function countUsbDevices(): Promise<number> {
  return FabricUsbSession.countFabricDevices();
}

export function usbHasSavedSerial(): boolean {
  return FabricUsbSession.hasSavedSerial();
}

export function clearUsbSavedPairing(): void {
  FabricUsbSession.clearSavedUsbPairing();
}
