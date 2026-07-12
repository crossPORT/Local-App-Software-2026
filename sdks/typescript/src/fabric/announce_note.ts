import type { ReceiveStatus } from './identity';
import { wirePortFromLeg } from './port';
import { makeInstanceId, type FabricSessionMessage } from './session_types';

export function buildAnnounceNote(
  leg: number,
  receiveStatus: ReceiveStatus,
  instanceId = makeInstanceId(),
): string {
  return `port=${wirePortFromLeg(leg)};receive=${receiveStatus};instance=${instanceId}`;
}

/** Parse peer receive policy from an announce note. Default ask_first (never invent open). */
export function receiveStatusFromAnnounceNote(note: string | undefined): ReceiveStatus {
  const raw = note?.match(/(?:^|;)\s*receive=([^;]+)/)?.[1]?.trim();
  if (raw === 'open' || raw === 'auto' || raw === 'auto_accept') return 'open';
  if (raw === 'busy') return 'busy';
  if (raw === 'ask_first') return 'ask_first';
  return 'ask_first';
}

export function buildAnnounceMessage(
  displayName: string,
  team: string,
  leg: number,
  receiveStatus: ReceiveStatus,
  instanceId = makeInstanceId(),
): FabricSessionMessage {
  return {
    kind: 'announce',
    from_name: displayName,
    team,
    session_id: makeInstanceId(),
    to_name: '',
    note: buildAnnounceNote(leg, receiveStatus, instanceId),
    payload_type: '',
    payload_name: '',
    file_count: 0,
    total_bytes: 0,
  };
}
