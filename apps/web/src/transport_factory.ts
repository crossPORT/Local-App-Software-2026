import {
  clearSavedUsbPairing,
  countFabricDevices,
  createFabricTransport,
  hasSavedSerial,
  type FabricTransport,
} from '@rocketbox/sdk';
import { fabricPortFromUrl, fabricSimEnabled } from './sim_flags';

/** PWA captures ?simulate= / ?port= and asks the SDK for a transport. */
export function createTransportSession(): FabricTransport {
  return createFabricTransport({
    simulate: fabricSimEnabled(),
    port: fabricPortFromUrl(),
  });
}

export async function countTransportDevices(): Promise<number> {
  if (fabricSimEnabled()) {
    return 4;
  }
  return countFabricDevices();
}

export function transportHasSavedSerial(): boolean {
  if (fabricSimEnabled()) {
    return false;
  }
  return hasSavedSerial();
}

export function clearTransportSavedPairing(): void {
  clearSavedUsbPairing();
}
