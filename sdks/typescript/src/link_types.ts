import type { FabricSessionMessage } from './session_types';

export const SESSION_SEND_TIMEOUT_MS = 2500;
export const LISTEN_POLL_GAP_MS = 250;
export const HANDSHAKE_POLL_GAP_MS = 50;
export const IN_SETTLE_MS = 120;
export const IN_FLIGHT_DRAIN_MS = 600;
export const ACTIVE_IN_POLL_WAIT_MS = 900;
export const INTERFACE_RECOVERY_TIMEOUT_MS = 1500;
export const SESSION_BODY_IN_TIMEOUT_MS = 800;
export const ALWAYS_LISTEN_HEADER_TIMEOUT_MS = 200;
export const PAYLOAD_CHUNK_TIMEOUT_MS = 30000;

export type Tier = 0 | 1;
export type ListenMode = 'always' | 'handshake' | 'off';

export type FabricLinkEvent =
  | { type: 'session'; message: FabricSessionMessage }
  | { type: 'error'; error: Error };

export type ControlJob = {
  label: string;
  priority: number;
  run: () => Promise<void>;
  resolve: () => void;
  reject: (err: unknown) => void;
};
