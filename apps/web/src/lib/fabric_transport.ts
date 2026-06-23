import type { FabricSessionMessage } from './fabric_session';
import type { ParsedHeader } from './fabric_protocol';
import type { ListenMode } from './fabric_link';

/** App-layer transport contract — implemented by WebUSB and by apps/web/sim. */
export interface FabricTransport {
  readonly connected: boolean;

  /** Fabric leg of the connected cable (from USB serial). */
  getFabricPortIndex(): number;

  getFabricLeg(): number;
  getSerialNumber(): string;

  connect(): Promise<string>;
  reconnectKnown(): Promise<string>;
  describeDevice(): string;
  disconnect(): Promise<void>;
  forgetThisDevice(): Promise<void>;
  resetConnection(): Promise<string>;
  ownsDevice(usbDevice: USBDevice): boolean;
  markDisconnected(): void;

  setListenMode(mode: ListenMode): void;
  ensureListening(): void;
  subscribeSession(handler: (message: FabricSessionMessage) => void): () => void;

  sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename?: string,
  ): Promise<void>;
  receiveHeader(): Promise<ParsedHeader>;
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
    expectedBytes?: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }>;
  prepareForPayloadSend(): Promise<void>;
  waitForIdle(): Promise<void>;
}
