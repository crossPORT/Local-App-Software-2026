/** Minimal WebUSB typings for the RocketBox TypeScript SDK. */

interface USBEndpoint {
  readonly endpointNumber: number;
  readonly direction: 'in' | 'out';
  readonly type: 'bulk' | 'interrupt' | 'isochronous' | 'control';
}

interface USBAlternateInterface {
  readonly endpoints: ReadonlyArray<USBEndpoint>;
}

interface USBInterface {
  readonly alternate: USBAlternateInterface;
}

interface USBConfiguration {
  readonly interfaces: ReadonlyArray<USBInterface>;
}

interface USBInTransferResult {
  readonly status: 'ok' | 'stall' | 'babble';
  readonly data?: DataView;
}

interface USBOutTransferResult {
  readonly status: 'ok' | 'stall' | 'babble';
  readonly bytesWritten: number;
}

interface USBDevice {
  readonly opened: boolean;
  readonly vendorId: number;
  readonly productId: number;
  readonly serialNumber?: string;
  readonly configuration: USBConfiguration | null;
  open(): Promise<void>;
  close(): Promise<void>;
  selectConfiguration(configurationValue: number): Promise<void>;
  claimInterface(interfaceNumber: number): Promise<void>;
  releaseInterface(interfaceNumber: number): Promise<void>;
  reset(): Promise<void>;
  clearHalt(direction: 'in' | 'out', endpointNumber: number): Promise<void>;
  transferIn(endpointNumber: number, length: number): Promise<USBInTransferResult>;
  transferOut(endpointNumber: number, data: BufferSource): Promise<USBOutTransferResult>;
  forget(): Promise<void>;
}

interface USBConnectionEvent extends Event {
  readonly device: USBDevice;
}

interface USB {
  getDevices(): Promise<USBDevice[]>;
  requestDevice(options: {
    filters: Array<{ vendorId: number; productId?: number }>;
  }): Promise<USBDevice>;
  addEventListener(
    type: 'connect' | 'disconnect',
    listener: (ev: USBConnectionEvent) => void,
  ): void;
  removeEventListener(
    type: 'connect' | 'disconnect',
    listener: (ev: USBConnectionEvent) => void,
  ): void;
}

interface Navigator {
  readonly usb?: USB;
}
