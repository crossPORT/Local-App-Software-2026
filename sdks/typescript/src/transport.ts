import type { FabricSessionMessage } from './session_types';
import type { ParsedHeader } from './protocol';
import type { ListenMode } from './link_types';

/** App-layer USB transport — WebUSB in this SDK; PWA sim may implement it locally. */
export interface RocketBoxTransport {
  readonly connected: boolean;
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

export type FabricTransport = RocketBoxTransport;
