import {
  clearSavedUsbPairing,
  countDevices,
  createRocketBoxTransport,
  hasSavedSerial,
  type RocketBoxTransport,
} from '@rocketbox/sdk';
import { fabricPortFromUrl, fabricSimEnabled } from './sim_flags';

/** PWA captures ?simulate= / ?port= and asks the SDK for a transport. */
export function createTransportSession(): RocketBoxTransport {
  return createRocketBoxTransport({
    simulate: fabricSimEnabled(),
    port: fabricPortFromUrl(),
  });
}

export async function countTransportDevices(): Promise<number> {
  if (fabricSimEnabled()) {
    return 4;
  }
  return countDevices();
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
