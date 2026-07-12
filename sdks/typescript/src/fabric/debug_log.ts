/** Optional fabric debug log (PWA wires boothLog via setFabricDebugLog). */
export type FabricDebugLevel = 'off' | 'normal' | 'verbose';

type LogFn = (port: number, event: string, detail?: string) => void;

let level: FabricDebugLevel = 'off';
let logFn: LogFn = () => undefined;

export function setFabricDebugLog(fn: LogFn, nextLevel: FabricDebugLevel = 'normal'): void {
  logFn = fn;
  level = nextLevel;
}

export function setFabricDebugLevel(next: FabricDebugLevel): void {
  level = next;
}

export function getFabricDebugLevel(): FabricDebugLevel {
  return level;
}

export function fabricDebugLog(leg: number, event: string, detail = ''): void {
  if (level === 'off') {
    return;
  }
  // `leg` is fabric index 0–3; boothLog converts to display Port 1–4.
  logFn(leg, event, detail);
}
