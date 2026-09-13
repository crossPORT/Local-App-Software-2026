import type { FabricTransport } from '@rocketbox/sdk';
import {
  clearUsbSavedPairing,
  countUsbDevices,
  createRocketBoxUsbTransport,
  usbHasSavedSerial,
} from '@rocketbox/sdk';
import { fabricSimEnabled } from '../sim/fabric_sim';
import { FabricSimSession } from '../sim/fabric_sim_session';

export function createTransportSession(): FabricTransport {
  if (fabricSimEnabled()) {
    return new FabricSimSession();
  }
  return createRocketBoxUsbTransport();
}

export async function countTransportDevices(): Promise<number> {
  if (fabricSimEnabled()) {
    return FabricSimSession.countFabricDevices();
  }
  return countUsbDevices();
}

export function transportHasSavedSerial(): boolean {
  if (fabricSimEnabled()) {
    return FabricSimSession.hasSavedSerial();
  }
  return usbHasSavedSerial();
}

export function clearTransportSavedPairing(): void {
  if (fabricSimEnabled()) {
    return;
  }
  clearUsbSavedPairing();
}
