/** Optional debug log (PWA wires eventLog via setDebugLog). */
export type DebugLevel = 'off' | 'normal' | 'verbose';

type LogFn = (portIndex: number, event: string, detail?: string) => void;

let sink: LogFn | null = null;
let level: DebugLevel = 'off';

export function setDebugLog(fn: LogFn, nextLevel: DebugLevel = 'normal'): void {
  sink = fn;
  level = nextLevel;
}

export function setDebugLevel(next: DebugLevel): void {
  level = next;
}

export function getDebugLevel(): DebugLevel {
  return level;
}

export function debugLog(portIndex: number, event: string, detail = ''): void {
  if (level === 'off' || !sink) {
    return;
  }
  // portIndex is 0–3; eventLog converts to display Port 1–4.
  if (level === 'verbose' || !event.startsWith('usb_')) {
    sink(portIndex, event, detail);
  }
}
