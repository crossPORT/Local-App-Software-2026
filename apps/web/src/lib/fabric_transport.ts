import type { FabricSessionMessage } from './fabric_session';

/** App-layer transport contract — implemented by WebUSB and by apps/web/sim. */
export interface FabricTransport {
  readonly connected: boolean;

  /** Fabric sort index of the connected cable (from announce wire format). */
  getFabricPortIndex(): number;

  connect(): Promise<string>;
  reconnectKnown(): Promise<string>;
  describeDevice(): string;
  disconnect(): Promise<void>;
  forgetThisDevice(): Promise<void>;
  resetConnection(): Promise<string>;
  ownsDevice(usbDevice: USBDevice): boolean;
  markDisconnected(): void;

  sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename?: string,
  ): Promise<void>;
  receiveHeader(): Promise<{ fileSize: number; filename: string }>;
  receivePayload(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array>;
  discardPayload(fileSize: number): Promise<void>;
  receiveBytes(onProgress?: (done: number, total: number) => void): Promise<Uint8Array>;
  sendSessionMessage(message: FabricSessionMessage): Promise<void>;
  tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null>;
  receiveFileTransfer(
    headerTimeoutMs: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }>;
  /** Abort a stuck session poll and drain the USB queue before payload OUT. */
  prepareForPayloadSend(): Promise<void>;
}
