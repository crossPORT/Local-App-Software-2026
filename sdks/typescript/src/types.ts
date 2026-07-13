import type { SystemInfo } from './system_info';
import type { ParsedHeader } from './protocol';
import type { SessionMessage } from './session_types';

export type ListenMode = 'always' | 'handshake' | 'off';

export type AnnouncePresenceMode = 'burst' | 'rotate';

/** Connect / disconnect / pairing / describe. */
export interface RocketBoxLink {
  readonly connected: boolean;
  getPortIndex(): number;
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
  subscribeConnect?(handler: () => void): () => void;
}

/** ROCKETBX payload send/receive. */
export interface RocketBoxDataPlane {
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
  receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes?: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }>;
  prepareForPayloadSend(): Promise<void>;
  waitForIdle(): Promise<void>;
}

/** Session messages, systems roster, listen mode. */
export interface RocketBoxSessionPlane {
  setListenMode(mode: ListenMode): void;
  ensureListening(): void;
  subscribeSession(handler: (message: SessionMessage) => void): () => void;
  listSystems?(): SystemInfo[] | Promise<SystemInfo[]>;
  syncSystems(handler: (systems: SystemInfo[]) => void): Promise<void>;
  ensureCircuit(peerSystemId: string): Promise<void>;
  /** Clear EP4 switch (dest=0). Call after transfer completes. */
  clearCircuit?(): Promise<void>;
  /** Current EP4 dest 1–4, or 0 if cleared / unknown. */
  switchDest?(): number;
  sendSessionMessage(message: SessionMessage): Promise<void>;
  /** Presence announce: burst (connect/force) or rotate (scheduled). */
  sendAnnouncePresence?(message: SessionMessage, mode: AnnouncePresenceMode): Promise<void>;
  tryReceiveSessionMessage(headerTimeoutMs: number): Promise<SessionMessage | null>;
}

/** App-layer transport contract (link + data + session). */
export type RocketBoxTransport = RocketBoxLink & RocketBoxDataPlane & RocketBoxSessionPlane;
