import type { ReceiveStatus } from './types';

export function buildAnnounceNote(portIndex: number, receiveStatus: ReceiveStatus): string {
  return `port=${portIndex};receive=${receiveStatus}`;
}

export function parseAnnounceNote(
  note: string,
  defaultPort: number,
): { portIndex: number; receiveStatus: ReceiveStatus } {
  let portIndex = defaultPort;
  let receiveStatus: ReceiveStatus = 'ask_first';

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
    }
  }

  return { portIndex, receiveStatus };
}
