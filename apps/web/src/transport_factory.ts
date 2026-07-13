import {
  clearSavedUsbPairing,
  countDevices,
  createRocketBoxTransport,
  hasSavedSerial,
  type RocketBoxTransport,
} from '@rocketbox/sdk';
import { portFromUrl, simEnabled } from './sim_flags';

/** PWA captures ?simulate= / ?port= and asks the SDK for a transport. */
export function createTransportSession(): RocketBoxTransport {
  return createRocketBoxTransport({
    simulate: simEnabled(),
    port: portFromUrl(),
  });
}

export async function countTransportDevices(): Promise<number> {
  if (simEnabled()) {
    return 4;
  }
  return countDevices();
}

export function transportHasSavedSerial(): boolean {
  if (simEnabled()) {
    return false;
  }
  return hasSavedSerial();
}

export function clearTransportSavedPairing(): void {
  clearSavedUsbPairing();
}
