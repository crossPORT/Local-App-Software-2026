import { remotePortIndexes } from '@rocketbox/sdk';
import type { IdentityProfile, PeerEntry } from './types';

export interface RosterSlot {
  leg: number;
  peer: PeerEntry | null;
}

/** Three fixed remote-leg slots; missing peers are null (grey in UI). */
export function rosterSlots(
  peers: PeerEntry[],
  usbConnected: boolean,
  _self: Pick<IdentityProfile, 'display_name'>,
  localLeg: number,
): RosterSlot[] {
  if (!usbConnected || localLeg < 0) {
    return [];
  }
  return remotePortIndexes(localLeg).map((leg) => {
    const peer =
      peers.find(
        (entry) =>
          entry.online &&
          entry.port_index === leg,
      ) ?? null;
    return { leg, peer };
  });
}
