import {
  sessionKindFromString,
  sessionKindToString,
  type FabricSessionMessage,
  type SessionMessageKind,
} from './session_types';

const SESSION_HEADER = 'FABRIC-SESSION-v1\n';

function trim(value: string): string {
  return value.trim();
}

function parseLine(line: string, out: FabricSessionMessage): void {
  const eq = line.indexOf('=');
  if (eq < 0) {
    return;
  }
  const key = trim(line.slice(0, eq));
  const value = trim(line.slice(eq + 1));
  switch (key) {
    case 'kind':
      out.kind = sessionKindFromString(value);
      break;
    case 'from':
      out.from_name = value;
      break;
    case 'team':
      out.team = value;
      break;
    case 'session_id':
      out.session_id = value;
      break;
    case 'to':
      out.to_name = value;
      break;
    case 'note':
      out.note = value;
      break;
    case 'payload_type':
      out.payload_type = value;
      break;
    case 'payload_name':
      out.payload_name = value;
      break;
    case 'files':
      out.file_count = Number.parseInt(value, 10) || 0;
      break;
    case 'total_bytes':
      out.total_bytes = Number.parseInt(value, 10) || 0;
      break;
    default:
      break;
  }
}

export function serializeSessionMessage(message: FabricSessionMessage): Uint8Array {
  const lines = [
    SESSION_HEADER.trimEnd(),
    `kind=${sessionKindToString(message.kind)}`,
    `from=${message.from_name}`,
    `team=${message.team}`,
    `session_id=${message.session_id}`,
  ];
  if (message.to_name) {
    lines.push(`to=${message.to_name}`);
  }
  if (message.note) {
    lines.push(`note=${message.note}`);
  }
  if (message.payload_type) {
    lines.push(`payload_type=${message.payload_type}`);
  }
  if (message.payload_name) {
    lines.push(`payload_name=${message.payload_name}`);
  }
  if (message.file_count > 0) {
    lines.push(`files=${message.file_count}`);
  }
  if (message.total_bytes > 0) {
    lines.push(`total_bytes=${message.total_bytes}`);
  }
  return new TextEncoder().encode(`${lines.join('\n')}\n`);
}

export function parseSessionPayload(data: Uint8Array): FabricSessionMessage | null {
  const payload = new TextDecoder().decode(data);
  if (!payload.startsWith(SESSION_HEADER)) {
    return null;
  }
  const out: FabricSessionMessage = {
    kind: 'unknown',
    from_name: '',
    team: '',
    session_id: '',
    to_name: '',
    note: '',
    payload_type: '',
    payload_name: '',
    file_count: 0,
    total_bytes: 0,
  };
  for (const rawLine of payload.slice(SESSION_HEADER.length).split('\n')) {
    const line = rawLine.endsWith('\r') ? rawLine.slice(0, -1) : rawLine;
    if (!line) {
      continue;
    }
    parseLine(line, out);
  }
  if (out.kind === 'unknown' || !out.session_id) {
    return null;
  }
  return out;
}

export function buildSessionReply(
  request: FabricSessionMessage,
  kind: Exclude<SessionMessageKind, 'offer' | 'announce' | 'unknown'>,
  fromName: string,
  team: string,
): FabricSessionMessage {
  return {
    kind,
    session_id: request.session_id,
    from_name: fromName,
    team,
    to_name: request.from_name,
    note: '',
    payload_type: '',
    payload_name: '',
    file_count: 0,
    total_bytes: 0,
  };
}

export type { FabricSessionMessage, SessionMessageKind } from './session_types';
export {
  makeInstanceId,
  makeSessionId,
  sessionKindFromString,
  sessionKindToString,
} from './session_types';
