import type { Connection } from '../connection';
import type { Session } from '../session_core';
import type { FabricSessionMessage } from './session_types';
import type { CircuitDataBuffer } from './rocketbox_buffer';
import { sendFileOnCircuit, sendSessionOnCircuit } from './rocketbox_io';
import {
  recvBytes,
  recvFileTransfer,
  recvHeader,
  recvPayload,
  tryRecvSession,
} from './rocketbox_recv';
import { bindCircuitConnection, type ConnHooks } from './rocketbox_wire';

export type PlaneState = {
  session: Session | null;
  connection: Connection | null;
  dataBuf: CircuitDataBuffer;
  sessionHandlers: Set<(m: FabricSessionMessage) => void>;
  hooks: ConnHooks;
};

export async function planeEnsureCircuit(state: PlaneState, peerSystemId: string): Promise<void> {
  if (!state.session) throw new Error('Not attached');
  if (state.connection?.state === 'open' && state.connection.peerSystemId === peerSystemId) {
    return;
  }
  if (state.connection) {
    try {
      await state.connection.close();
    } catch {
      /* best effort */
    }
  }
  const conn = await state.session.connect(peerSystemId);
  state.connection = conn;
  bindCircuitConnection(conn, state.dataBuf, state.sessionHandlers, state.hooks, () => {
    if (state.connection === conn) state.connection = null;
  });
}

export async function planeSendSession(
  state: PlaneState,
  message: FabricSessionMessage,
): Promise<void> {
  if (!state.connection || state.connection.state !== 'open') {
    throw new Error('No circuit — call ensureCircuit first');
  }
  await sendSessionOnCircuit(state.connection, message);
}

export async function planeSendBytes(
  state: PlaneState,
  payload: Uint8Array,
  onProgress?: (done: number, total: number) => void,
  filename = 'payload.bin',
): Promise<void> {
  if (!state.connection || state.connection.state !== 'open') throw new Error('No circuit');
  await sendFileOnCircuit(state.connection, payload, filename, onProgress);
}

export const planeRecv = {
  file: recvFileTransfer,
  header: recvHeader,
  payload: recvPayload,
  bytes: recvBytes,
  trySession: tryRecvSession,
};
