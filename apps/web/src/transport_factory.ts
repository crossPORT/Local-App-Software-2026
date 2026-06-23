import type { FabricTransport } from './lib/fabric_transport';
import { FabricUsbSession } from './lib/fabric_usb';
import { fabricSimEnabled } from '../sim/fabric_sim';
import { FabricSimSession } from '../sim/fabric_sim_session';

export function createTransportSession(): FabricTransport {
  if (fabricSimEnabled()) {
    return new FabricSimSession();
  }
  return new FabricUsbSession();
}

export async function countTransportDevices(): Promise<number> {
  if (fabricSimEnabled()) {
    return FabricSimSession.countFabricDevices();
  }
  return FabricUsbSession.countFabricDevices();
}

export function transportHasSavedSerial(): boolean {
  if (fabricSimEnabled()) {
    return FabricSimSession.hasSavedSerial();
  }
  return FabricUsbSession.hasSavedSerial();
}

export function clearTransportSavedPairing(): void {
  if (fabricSimEnabled()) {
    return;
  }
  FabricUsbSession.clearSavedUsbPairing();
}
