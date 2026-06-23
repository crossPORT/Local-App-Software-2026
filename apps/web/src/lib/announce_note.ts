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
  let note = `port=${portIndex};receive=${receiveStatus}`;
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
      if (Number.isFinite(parsed) && parsed >= 0) {
        portIndex = parsed;
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

/** Map an announced port to a remote fabric port (never our own slot). */
export function resolveRemoteFabricPort(myPort: number, announcedPort: number): number {
  const otherPort = myPort === 0 ? 1 : 0;
  return announcedPort === myPort ? otherPort : announcedPort;
}
