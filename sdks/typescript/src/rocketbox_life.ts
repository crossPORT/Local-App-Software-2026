import type { Session } from './session_core';
import type { Transport } from './transport';
import { attachSession } from './rocketbox_attach';
import type { PlaneState } from './rocketbox_plane';
import { bindCircuitConnection, clearConnHooks, type ConnHooks } from './rocketbox_wire';

export type LifeBag = {
  plane: PlaneState;
  hooks: ConnHooks;
  transport: Transport | null;
  unsubIncoming: (() => void) | null;
  unsubDetached: (() => void) | null;
  onLost?: () => void;
  port: number;
};

export async function teardownPlane(bag: LifeBag): Promise<void> {
  bag.unsubDetached?.();
  bag.unsubDetached = null;
  bag.unsubIncoming?.();
  clearConnHooks(bag.hooks);
  try {
    await bag.plane.connection?.close();
  } catch {
    /* best effort */
  }
  bag.plane.connection = null;
  try {
    await bag.transport?.disconnect();
  } catch {
    /* best effort */
  }
  bag.plane.session = null;
  bag.plane.dataBuf.clear();
}

export async function attachOrReplace(
  bag: LifeBag & { connectHandlers: Set<() => void> },
): Promise<{
  hub: Session;
  transport: Transport;
  port: number;
  unsubIncoming: () => void;
  unsubDetached: () => void;
}> {
  await teardownPlane(bag);
  const attached = await attachSession({ port: bag.port });
  bag.plane.session = attached.session;
  bag.transport = attached.transport;
  const unsubIncoming = attached.session.onIncomingCircuit((c) => {
    bag.plane.connection = c;
    bindCircuitConnection(c, bag.plane.dataBuf, bag.plane.sessionHandlers, bag.hooks, () => {
      if (bag.plane.connection === c) {
        bag.plane.connection = null;
      }
    });
  });
  const unsubDetached = attached.session.onDetached(() => bag.onLost?.());
  bag.unsubDetached = unsubDetached;
  bag.connectHandlers.forEach((h) => h());
  return {
    hub: attached.session,
    transport: attached.transport,
    port: bag.port,
    unsubIncoming,
    unsubDetached,
  };
}
