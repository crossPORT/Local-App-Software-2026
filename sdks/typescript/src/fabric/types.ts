import type { SystemInfo } from '../system_info';
import type { ParsedHeader } from './protocol';
import type { FabricSessionMessage } from './session_types';

export type ListenMode = 'always' | 'handshake' | 'off';

/** App-layer transport contract for the RocketBox Session SDK. */
export interface FabricTransport {
  readonly connected: boolean;

  getFabricPortIndex(): number;
  getFabricLeg(): number;
  getSerialNumber(): string;
  getSystemId(): string;

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
  subscribeConnect?(handler: () => void): () => void;
  listSystems?(): SystemInfo[] | Promise<SystemInfo[]>;
  syncSystems(handler: (systems: SystemInfo[]) => void): Promise<void>;
  ensureCircuit(peerSystemId: string): Promise<void>;

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
