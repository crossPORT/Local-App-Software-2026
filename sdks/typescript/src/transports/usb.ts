import type { Transport } from '../transport';

export class UsbTransport implements Transport {
  get connected(): boolean {
    return false;
  }

  async init(): Promise<void> {
    throw new Error('WebUSB Transport is not enabled in simulation environments.');
  }

  async disconnect(): Promise<void> {
    // Release WebUSB interface
  }

  writeEP1(_data: Uint8Array): void {
    // device.transferOut(1, data)
  }

  writeEP4(_data: Uint8Array): Promise<[DataView, Uint8Array]> {
    // device.transferOut(4, data) -> wait on transferIn(3)
    throw new Error('WebUSB not supported in simulator build');
  }

  onEP2Received(_callback: (data: Uint8Array) => void): void {
    // device.transferIn(2) loop
  }

  onEP3Received(_callback: (header: DataView, payload: Uint8Array) => void): void {
    // device.transferIn(3) loop
  }
}
