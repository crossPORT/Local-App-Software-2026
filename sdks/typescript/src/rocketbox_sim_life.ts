import type { Session } from './session_core';
import type { Transport } from './transport';
import { CircuitDataBuffer } from './rocketbox_buffer';
import { attachOrReplace, teardownPlane } from './rocketbox_life';
import type { PlaneState } from './rocketbox_plane';
import type { ConnHooks } from './rocketbox_wire';
import { formatPortLabel } from './port';

export type SimBag = {
  plane: PlaneState;
  hooks: ConnHooks;
  transport: Transport | null;
  unsubIncoming: (() => void) | null;
  unsubDetached: (() => void) | null;
  connectHandlers: Set<() => void>;
  port: number;
  hub: Session | null;
  onLost: () => void;
  describe: () => string;
};

export async function simAttach(bag: SimBag): Promise<{
  hub: Session;
  transport: Transport;
  port: number;
  unsubIncoming: () => void;
  unsubDetached: () => void;
}> {
  await teardownPlane({
    plane: bag.plane,
    hooks: bag.hooks,
    transport: bag.transport,
    unsubIncoming: bag.unsubIncoming,
    unsubDetached: bag.unsubDetached,
    port: bag.port,
  });
  return attachOrReplace({
    plane: bag.plane,
    hooks: bag.hooks,
    transport: null,
    unsubIncoming: null,
    unsubDetached: null,
    connectHandlers: bag.connectHandlers,
    port: bag.port,
    onLost: bag.onLost,
  });
}

export async function simTeardown(bag: SimBag): Promise<void> {
  await teardownPlane({
    plane: bag.plane,
    hooks: bag.hooks,
    transport: bag.transport,
    unsubIncoming: bag.unsubIncoming,
    unsubDetached: bag.unsubDetached,
    port: bag.port,
  });
}

export function emptyPlane(hooks: ConnHooks): PlaneState {
  return {
    session: null,
    connection: null,
    dataBuf: new CircuitDataBuffer(),
    sessionHandlers: new Set(),
    hooks,
  };
}

export function describeSimPort(port: number): string {
  return formatPortLabel(port - 1, `sdk-port-${port}`);
}
