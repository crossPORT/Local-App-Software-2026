import { EP_IN, EP_OUT, INTERFACE_NUMBER } from '../src/lib/fabric_protocol';
import { fabricHubReceiveBuffer, fabricHubTransmit } from './fabric_hub';

const READ_TIMEOUT_MS = 5000;

/** Minimal USBDevice shim for FabricLink in sim mode. */
export class SimUsbDevice {
  opened = true;
  configuration = 1;
  productName = 'SLS USB DEVICE (sim)';

  constructor(readonly serialNumber: string) {}

  async open(): Promise<void> {
    this.opened = true;
  }

  async close(): Promise<void> {
    this.opened = false;
  }

  async selectConfiguration(_configurationValue: number): Promise<void> {}

  async claimInterface(_interfaceNumber: number): Promise<void> {}

  async releaseInterface(_interfaceNumber: number): Promise<void> {}

  async transferIn(_endpointNumber: number, byteCount: number) {
    if (_endpointNumber !== EP_IN) {
      throw new Error('Sim only supports bulk IN');
    }
    const data = await fabricHubReceiveBuffer(this.serialNumber).readUpTo(byteCount, READ_TIMEOUT_MS);
    return {
      status: 'ok' as const,
      data: new DataView(data.buffer, data.byteOffset, data.byteLength),
    };
  }

  async transferOut(_endpointNumber: number, data: BufferSource) {
    if (_endpointNumber !== EP_OUT) {
      throw new Error('Sim only supports bulk OUT');
    }
    const bytes =
      data instanceof ArrayBuffer
        ? new Uint8Array(data)
        : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    fabricHubTransmit(this.serialNumber, bytes);
    return { status: 'ok' as const, bytesWritten: bytes.length };
  }
}

void INTERFACE_NUMBER;
