import type { Session } from './session_core';
import type { Transport } from './transport';
import type { ConnHooks } from './rocketbox_wire';
import type { PlaneState } from './rocketbox_plane';
import { simAttach, simTeardown } from './rocketbox_sim_life';

export type SimHostState = {
  plane: PlaneState;
  hooks: ConnHooks;
  transport: Transport | null;
  unsubIncoming: (() => void) | null;
  unsubDetached: (() => void) | null;
  connectHandlers: Set<() => void>;
  port: number;
  hub: Session | null;
  describe: () => string;
  onLost: () => void;
};

export async function hostSimConnect(s: SimHostState): Promise<string> {
  const next = await simAttach({
    plane: s.plane,
    hooks: s.hooks,
    transport: s.transport,
    unsubIncoming: s.unsubIncoming,
    unsubDetached: s.unsubDetached,
    connectHandlers: s.connectHandlers,
    port: s.port,
    hub: s.hub,
    onLost: s.onLost,
    describe: s.describe,
  });
  s.hub = next.hub;
  s.transport = next.transport;
  s.port = next.port;
  s.unsubIncoming = next.unsubIncoming;
  s.unsubDetached = next.unsubDetached;
  return s.describe();
}

export async function hostSimTeardown(s: SimHostState): Promise<void> {
  await simTeardown({
    plane: s.plane,
    hooks: s.hooks,
    transport: s.transport,
    unsubIncoming: s.unsubIncoming,
    unsubDetached: s.unsubDetached,
    connectHandlers: s.connectHandlers,
    port: s.port,
    hub: s.hub,
    onLost: () => undefined,
    describe: s.describe,
  });
  s.unsubIncoming = null;
  s.unsubDetached = null;
  s.transport = null;
  s.hub = null;
}
