import { FABRIC_LEG_COUNT, legFromWirePort, wirePortFromLeg } from './fabric_port';
import type { ReceiveStatus } from './types';

export interface ParsedAnnounceNote {
  portIndex: number;
  receiveStatus: ReceiveStatus;
  instanceId: string;
}

export function buildAnnounceNote(
  portIndex: number,
  receiveStatus: ReceiveStatus,
  instanceId = '',
): string {
  let note = `port=${wirePortFromLeg(portIndex)};receive=${receiveStatus}`;
  if (instanceId) {
    note += `;instance=${instanceId}`;
  }
  return note;
}

export function parseAnnounceNote(note: string, defaultPort: number): ParsedAnnounceNote {
  let portIndex = defaultPort;
  let receiveStatus: ReceiveStatus = 'ask_first';
  let instanceId = '';

  for (const token of note.split(';')) {
    const trimmed = token.trim();
    if (trimmed.startsWith('port=')) {
      const parsed = Number.parseInt(trimmed.slice('port='.length), 10);
      const leg = legFromWirePort(parsed);
      if (leg !== null) {
        portIndex = leg;
      }
      continue;
    }
    if (trimmed.startsWith('receive=')) {
      const value = trimmed.slice('receive='.length);
      if (value === 'open' || value === 'auto' || value === 'auto_accept') {
        receiveStatus = 'open';
      } else if (value === 'busy') {
        receiveStatus = 'busy';
      } else {
        receiveStatus = 'ask_first';
      }
      continue;
    }
    if (trimmed.startsWith('instance=')) {
      instanceId = trimmed.slice('instance='.length).trim();
    }
  }

  return { portIndex, receiveStatus, instanceId };
}

/** Remote leg from peer announce; null if echo or invalid. */
export function resolveRemoteFabricLeg(myLeg: number, announcedLeg: number): number | null {
  if (announcedLeg === myLeg) {
    return null;
  }
  if (announcedLeg < 0 || announcedLeg >= FABRIC_LEG_COUNT) {
    return null;
  }
  return announcedLeg;
}

/** @deprecated use resolveRemoteFabricLeg */
export function resolveRemoteFabricPort(myPort: number, announcedPort: number): number {
  return resolveRemoteFabricLeg(myPort, announcedPort) ?? announcedPort;
}
