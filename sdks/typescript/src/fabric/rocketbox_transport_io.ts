import type { HwPlane } from '../transports/hw_plane';
import type { PlaneState } from './rocketbox_plane';
import { planeIo } from './rocketbox_plane_io';
import type { FabricSessionMessage } from './session_types';
import type { ParsedHeader } from './protocol';

export type IoBackend = {
  simulate: boolean;
  hw: HwPlane | null;
  plane: PlaneState;
};

export function transportIo(bag: IoBackend) {
  const io = planeIo(bag.plane);
  return {
    ensureCircuit(id: string): Promise<void> {
      return bag.simulate ? io.ensureCircuit(id) : bag.hw!.ensureCircuit(id);
    },
    sendSessionMessage(m: FabricSessionMessage): Promise<void> {
      return bag.simulate ? io.sendSessionMessage(m) : bag.hw!.sendSessionMessage(m);
    },
    sendBytes(
      payload: Uint8Array,
      onProgress?: (done: number, total: number) => void,
      filename = 'payload.bin',
    ): Promise<void> {
      return bag.simulate
        ? io.sendBytes(payload, onProgress, filename)
        : bag.hw!.sendBytes(payload, onProgress, filename);
    },
    receiveFileTransfer(
      ms: number,
      expected?: number,
      onProgress?: (d: number, t: number) => void,
    ): Promise<{ data: Uint8Array; filename: string }> {
      return bag.simulate
        ? io.receiveFileTransfer(ms, expected, onProgress)
        : bag.hw!.receiveFileTransfer(ms, expected, onProgress);
    },
    receiveHeader(): Promise<ParsedHeader> {
      return io.receiveHeader();
    },
    receivePayload(n: number, onProgress?: (d: number, t: number) => void): Promise<Uint8Array> {
      return io.receivePayload(n, onProgress);
    },
    discardPayload(n: number): Promise<void> {
      return io.discardPayload(n);
    },
    receiveBytes(onProgress?: (d: number, t: number) => void): Promise<Uint8Array> {
      return io.receiveBytes(onProgress);
    },
    tryReceiveSessionMessage(ms: number): Promise<FabricSessionMessage | null> {
      return io.tryReceiveSessionMessage(ms);
    },
  };
}
