import type { SystemInfo } from '../system_info';
import { debugLog } from '../debug_log';
import { portIndexFromWire } from '../port';
import { receiveStatusFromAnnounceNote } from '../announce_note';
import type { SessionMessage } from '../session_types';

/** Apply inbound announce to the peer map (skip self / bad port). */
export function noteAnnouncePeer(
  msg: SessionMessage,
  myLeg: number,
  peers: Map<string, SystemInfo>,
): void {
  if (msg.kind !== 'announce' || !msg.from_name) return;
  const portMatch = (msg.note ?? '').match(/(?:^|;)\s*port=(\d+)/);
  const wire = portMatch ? Number.parseInt(portMatch[1]!, 10) : NaN;
  const leg = portIndexFromWire(wire);
  if (leg == null || leg === myLeg) return;
  const id = `sys-port-${leg + 1}`;
  const receive = receiveStatusFromAnnounceNote(msg.note);
  peers.set(id, {
    id,
    name: msg.from_name,
    status: receive === 'busy' ? 'busy' : 'reachable',
    receive,
  });
  debugLog(myLeg, 'announce_received', `${msg.from_name} ${id}`);
}
