/** Minimal WebUSB typings for Chrome (not in default DOM lib). */

interface Navigator {
  readonly usb: USB;
}

interface USBConnectionEvent extends Event {
  readonly device: USBDevice;
}

interface USB {
  getDevices(): Promise<USBDevice[]>;
  requestDevice(options: USBDeviceRequestOptions): Promise<USBDevice>;
  addEventListener(
    type: 'connect' | 'disconnect',
    listener: (ev: USBConnectionEvent) => void,
  ): void;
  removeEventListener(
    type: 'connect' | 'disconnect',
    listener: (ev: USBConnectionEvent) => void,
  ): void;
}

interface USBDeviceRequestOptions {
  filters: USBDeviceFilter[];
}

interface USBDeviceFilter {
  vendorId?: number;
  productId?: number;
}

interface USBDevice {
  readonly opened: boolean;
  readonly vendorId: number;
  readonly productId: number;
  readonly productName?: string;
  readonly serialNumber?: string;
  readonly configuration: USBConfiguration | null;
  open(): Promise<void>;
  close(): Promise<void>;
  selectConfiguration(configurationValue: number): Promise<void>;
  claimInterface(interfaceNumber: number): Promise<void>;
  releaseInterface(interfaceNumber: number): Promise<void>;
  clearHalt(direction: 'in' | 'out', endpointNumber: number): Promise<void>;
  transferIn(endpointNumber: number, length: number): Promise<USBInTransferResult>;
  transferOut(endpointNumber: number, data: BufferSource): Promise<USBOutTransferResult>;
  forget(): Promise<void>;
}

interface USBConfiguration {
  readonly configurationValue: number;
}

interface USBInTransferResult {
  readonly data?: DataView;
  readonly status: 'ok' | 'stall' | 'babble';
}

interface USBOutTransferResult {
  readonly status: 'ok' | 'stall' | 'babble';
}
