import type { FabricTransport } from './lib/fabric_transport';
import { FabricUsbSession } from './lib/fabric_usb';

export function createTransportSession(portIndex: number): FabricTransport {
  return new FabricUsbSession(portIndex);
}

export async function countTransportDevices(): Promise<number> {
  return FabricUsbSession.countFabricDevices();
}

export function transportHasSavedSerial(): boolean {
  return FabricUsbSession.hasSavedSerial();
}
