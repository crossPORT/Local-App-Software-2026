import type { FabricTransport } from './lib/fabric_transport';
import { SdkFabricTransport } from './lib/sdk_fabric_transport';

export function createTransportSession(): FabricTransport {
  return new SdkFabricTransport();
}

export async function countTransportDevices(): Promise<number> {
  return SdkFabricTransport.countFabricDevices();
}

export function transportHasSavedSerial(): boolean {
  return SdkFabricTransport.hasSavedSerial();
}

export function clearTransportSavedPairing(): void {
  SdkFabricTransport.clearSavedUsbPairing();
}
