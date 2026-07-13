export type SessionMessageKind = 'offer' | 'accept' | 'decline' | 'ready' | 'announce' | 'unknown';

export interface SessionMessage {
  kind: SessionMessageKind;
  from_name: string;
  team: string;
  session_id: string;
  to_name: string;
  note: string;
  payload_type: string;
  payload_name: string;
  file_count: number;
  total_bytes: number;
}

export function sessionKindToString(kind: SessionMessageKind): string {
  switch (kind) {
    case 'offer':
      return 'offer';
    case 'accept':
      return 'accept';
    case 'decline':
      return 'decline';
    case 'ready':
      return 'ready';
    case 'announce':
      return 'announce';
    default:
      return 'unknown';
  }
}

export function sessionKindFromString(value: string): SessionMessageKind {
  switch (value) {
    case 'offer':
      return 'offer';
    case 'accept':
      return 'accept';
    case 'decline':
      return 'decline';
    case 'ready':
      return 'ready';
    case 'announce':
      return 'announce';
    default:
      return 'unknown';
  }
}

export function makeSessionId(): string {
  let out = '';
  for (let i = 0; i < 16; i += 1) {
    out += Math.floor(Math.random() * 16).toString(16);
  }
  return out;
}

/** Stable id for this app instance — distinguishes stations with the same display name. */
export function makeInstanceId(): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID().replace(/-/g, '').slice(0, 12);
  }
  return makeSessionId().slice(0, 12);
}
