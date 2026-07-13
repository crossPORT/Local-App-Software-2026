import { debugLog } from '../debug_log';
import { remoteDisplayPorts, toDisplayPort } from '../port';
import type { SessionMessage } from '../session_types';
import type { SwitchDestCache } from './switch_cache';
import type { UsbEndpoints } from './usb_ids';

export type AnnouncePresenceMode = 'burst' | 'rotate';

export type AnnouncePresenceBag = {
  device: USBDevice;
  eps: UsbEndpoints;
  portIndex: number;
  switches: SwitchDestCache;
  rotateIndex: { current: number };
  sendSession: (msg: SessionMessage) => Promise<void>;
};

/** Live switch dest 1–4, or null if cleared / unknown. */
export function connectedSwitchDest(switches: SwitchDestCache): number | null {
  const d = switches.last;
  return d != null && d >= 1 && d <= 4 ? d : null;
}

/**
 * Presence: switch → announce; leave last dest (no idle dest=0 clear).
 * Sticky `preserve` (session/listen) holds scheduled rotate on that dest.
 */
export async function sendAnnouncePresence(
  bag: AnnouncePresenceBag,
  message: SessionMessage,
  mode: AnnouncePresenceMode,
): Promise<void> {
  const held = connectedSwitchDest(bag.switches);
  const self = toDisplayPort(bag.portIndex);
  const remotes = remoteDisplayPorts(self);

  if (held != null && mode === 'rotate' && bag.switches.preserve) {
    debugLog(bag.portIndex, 'announce_hold', `dest=${held}`);
    await bag.sendSession(message);
    return;
  }

  if (remotes.length === 0) {
    await bag.sendSession(message);
    await finishAnnounce(bag, held);
    return;
  }

  if (mode === 'burst') {
    for (const dest of remotes) {
      debugLog(bag.portIndex, 'announce_fanout', `dest=${dest}`);
      await bag.switches.switchIfNeeded(bag.device, bag.eps, dest);
      await bag.sendSession(message);
    }
    bag.rotateIndex.current = 0;
    await finishAnnounce(bag, held);
    return;
  }

  const dest = remotes[bag.rotateIndex.current % remotes.length]!;
  bag.rotateIndex.current = (bag.rotateIndex.current + 1) % remotes.length;
  debugLog(bag.portIndex, 'announce_rotate', `dest=${dest}`);
  await bag.switches.switchIfNeeded(bag.device, bag.eps, dest);
  await bag.sendSession(message);
  await finishAnnounce(bag, held);
}

async function finishAnnounce(
  bag: AnnouncePresenceBag,
  held: number | null,
): Promise<void> {
  if (held != null && bag.switches.preserve) {
    debugLog(bag.portIndex, 'announce_restore', `dest=${held}`);
    await bag.switches.switchIfNeeded(bag.device, bag.eps, held);
    return;
  }
  const left = connectedSwitchDest(bag.switches);
  debugLog(
    bag.portIndex,
    'announce_leave',
    left != null ? `dest=${left}` : 'none',
  );
}
