import type { ParsedHeader } from './protocol';
import type { SessionMessage } from './session_types';
import {
  planeEnsureCircuit,
  planeRecv,
  planeSendBytes,
  planeSendSession,
  type PlaneState,
} from './rocketbox_plane';

/** Receive/send plane helpers shared by RocketBoxTransport. */
export function planeIo(plane: PlaneState) {
  return {
    ensureCircuit(id: string): Promise<void> {
      return planeEnsureCircuit(plane, id);
    },
    sendSessionMessage(message: SessionMessage): Promise<void> {
      return planeSendSession(plane, message);
    },
    sendBytes(
      payload: Uint8Array,
      onProgress?: (done: number, total: number) => void,
      filename = 'payload.bin',
    ): Promise<void> {
      return planeSendBytes(plane, payload, onProgress, filename);
    },
    receiveFileTransfer(
      ms: number,
      expected?: number,
      onProgress?: (d: number, t: number) => void,
    ): Promise<{ data: Uint8Array; filename: string }> {
      return planeRecv.file(plane.dataBuf, ms, expected, onProgress);
    },
    receiveHeader(): Promise<ParsedHeader> {
      return planeRecv.header(plane.dataBuf);
    },
    receivePayload(n: number, onProgress?: (d: number, t: number) => void): Promise<Uint8Array> {
      return planeRecv.payload(plane.dataBuf, n, onProgress);
    },
    discardPayload(n: number): Promise<void> {
      return planeRecv.payload(plane.dataBuf, n).then(() => undefined);
    },
    receiveBytes(onProgress?: (d: number, t: number) => void): Promise<Uint8Array> {
      return planeRecv.bytes(plane.dataBuf, onProgress);
    },
    tryReceiveSessionMessage(ms: number): Promise<SessionMessage | null> {
      return planeRecv.trySession(ms);
    },
  };
}
