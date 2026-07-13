import { toDisplayPort } from '@rocketbox/sdk';

export type EventLogLevel = 'off' | 'normal' | 'verbose';

const MAX_LINES = 400;
const STORAGE_KEY = 'rocketbox_debug_log';

const lines: string[] = [];
const listeners = new Set<() => void>();
let level: EventLogLevel = 'off';

function notifyListeners(): void {
  for (const listener of listeners) {
    listener();
  }
}

function readLevelFromUrl(): EventLogLevel | null {
  if (typeof window === 'undefined') {
    return null;
  }
  const raw = new URLSearchParams(window.location.search).get('debug_log');
  if (raw === 'verbose') {
    return 'verbose';
  }
  if (raw === '1' || raw === 'true' || raw === 'normal') {
    return 'normal';
  }
  return null;
}

function initLevel(): EventLogLevel {
  const fromUrl = readLevelFromUrl();
  if (fromUrl) {
    return fromUrl;
  }
  if (typeof localStorage !== 'undefined') {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved === 'verbose' || saved === 'normal') {
      return saved;
    }
  }
  return 'off';
}

level = initLevel();

export function getEventLogLevel(): EventLogLevel {
  return level;
}

export function setEventLogLevel(next: EventLogLevel): void {
  level = next;
  if (typeof localStorage !== 'undefined') {
    if (next === 'off') {
      localStorage.removeItem(STORAGE_KEY);
    } else {
      localStorage.setItem(STORAGE_KEY, next);
    }
  }
}

function isoTimestamp(): string {
  return new Date().toISOString();
}

export function eventLog(port: number, event: string, detail = ''): void {
  if (level === 'off') {
    return;
  }
  const portLabel = toDisplayPort(port);
  const line = detail
    ? `${isoTimestamp()} [port=${portLabel}] ${event} | ${detail}`
    : `${isoTimestamp()} [port=${portLabel}] ${event}`;
  lines.push(line);
  while (lines.length > MAX_LINES) {
    lines.shift();
  }
  notifyListeners();
  const mirror =
    level === 'verbose' ||
    (typeof localStorage !== 'undefined' && localStorage.getItem('rocketbox_console_mirror') === '1');
  if (mirror) {
    console.debug(line);
  }
}

export function subscribeEventLog(listener: () => void): () => void {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function readEventLogLines(maxLines = MAX_LINES): string[] {
  const start = Math.max(0, lines.length - maxLines);
  return lines.slice(start);
}

export function readBoothLogTail(maxLines = MAX_LINES): string {
  const slice = readEventLogLines(maxLines);
  if (slice.length === 0) {
    return '(log is empty)\n';
  }
  return `${slice.join('\n')}\n`;
}

export function clearBoothLog(): void {
  lines.length = 0;
  lines.push(`${isoTimestamp()} [boot] log cleared`);
  notifyListeners();
}

export function filterBoothLogLinesByPort(entries: string[], port: number | null): string[] {
  if (port === null) {
    return entries;
  }
  const needle = `[port=${toDisplayPort(port)}]`;
  return entries.filter((line) => line.includes(needle) || line.includes('[boot]'));
}

export function filterBoothLogByPort(text: string, port: number | null): string {
  const filtered = filterBoothLogLinesByPort(
    text.split('\n').filter((line) => line.length > 0),
    port,
  );
  if (filtered.length === 0) {
    return `(no lines for port ${port === null ? '?' : toDisplayPort(port)} yet)\n`;
  }
  return `${filtered.join('\n')}\n`;
}
