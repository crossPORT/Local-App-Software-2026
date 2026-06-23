import { remoteFabricLegs } from './fabric_port';
import type { IdentityProfile, PeerEntry } from './types';

export interface RosterSlot {
  leg: number;
  peer: PeerEntry | null;
}

/** Three fixed remote-leg slots; missing peers are null (grey in UI). */
export function rosterSlots(
  peers: PeerEntry[],
  fabricConnected: boolean,
  self: Pick<IdentityProfile, 'display_name'>,
  localLeg: number,
): RosterSlot[] {
  if (!fabricConnected || localLeg < 0) {
    return [];
  }
  return remoteFabricLegs(localLeg).map((leg) => {
    const peer =
      peers.find(
        (entry) =>
          entry.online &&
          entry.port_index === leg &&
          entry.display_name !== self.display_name.trim(),
      ) ?? null;
    return { leg, peer };
  });
}
