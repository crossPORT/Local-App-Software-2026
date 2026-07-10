import type { Session, Transport } from '@rocketbox/sdk';
import { attachSession, portFromUrl } from './sdk_fabric_attach';
import type { PlaneState } from './sdk_fabric_plane';
import { bindCircuitConnection, clearConnHooks, type ConnHooks } from './sdk_fabric_wire';

export type LifeBag = {
  plane: PlaneState;
  hooks: ConnHooks;
  transport: Transport | null;
  unsubIncoming: (() => void) | null;
  unsubDetached: (() => void) | null;
  onLost?: () => void;
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
  device: USBDevice | null;
  port: number;
  unsubIncoming: () => void;
  unsubDetached: () => void;
}> {
  await teardownPlane(bag);
  const port = portFromUrl();
  const attached = await attachSession(port);
  bag.plane.session = attached.session;
  const unsubIncoming = attached.session.onIncomingCircuit((c) => {
    bag.plane.connection = c;
    bindCircuitConnection(c, bag.plane.dataBuf, bag.plane.sessionHandlers, bag.hooks, () => {
      if (bag.plane.connection === c) bag.plane.connection = null;
    });
  });
  const unsubDetached = attached.session.onDetached(() => bag.onLost?.());
  bag.unsubDetached = unsubDetached;
  bag.connectHandlers.forEach((h) => h());
  return {
    hub: attached.session,
    transport: attached.transport,
    device: attached.device,
    port,
    unsubIncoming,
    unsubDetached,
  };
}
