import { getBoothLogLevel, readBoothLogLines, subscribeBoothLog } from './booth_log';

const SHIP_ENDPOINT = '/__booth/log';
const INTERVAL_MS = 2000;

/**
 * Given the previously shipped tail snapshot and the current tail, return only
 * the lines that are new. The booth log is a fixed-size ring buffer (old lines
 * are evicted from the front), so two snapshots overlap in the middle: a suffix
 * of `prev` matches a prefix of `curr`. We find the longest such overlap and
 * treat everything after it as new. This avoids duplicates and tolerates the
 * buffer rotating/shrinking without crashing.
 */
export function computeNewLines(prev: string[], curr: string[]): string[] {
  const maxOverlap = Math.min(prev.length, curr.length);
  for (let overlap = maxOverlap; overlap > 0; overlap -= 1) {
    let matches = true;
    for (let i = 0; i < overlap; i += 1) {
      if (prev[prev.length - overlap + i] !== curr[i]) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return curr.slice(overlap);
    }
  }
  return curr.slice();
}

function makeClientId(): string {
  const rand = Math.random().toString(36).slice(2, 10);
  return `tab-${rand}`;
}

let started = false;

/**
 * Start periodic shipping of booth log lines to the dev server's
 * `/__booth/log` endpoint so an operator/agent can watch all devices centrally.
 * Idempotent and safe to call in any environment (no-ops without window/fetch).
 */
export function startBoothLogShipper(): void {
  if (started) {
    return;
  }
  if (typeof window === 'undefined' || typeof fetch !== 'function') {
    return;
  }
  started = true;

  const clientId = makeClientId();
  let lastTail: string[] = [];
  let inFlight = false;

  async function ship(): Promise<void> {
    if (inFlight || getBoothLogLevel() === 'off') {
      return;
    }
    const curr = readBoothLogLines();
    const newLines = computeNewLines(lastTail, curr);
    if (newLines.length === 0) {
      return;
    }
    inFlight = true;
    try {
      await fetch(SHIP_ENDPOINT, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ clientId, lines: newLines }),
      });
      // Only advance the cursor after a successful POST so failures retry.
      lastTail = curr;
    } catch {
      // The endpoint may not exist (prod build) or the dev server may be down.
      // Never disrupt the app; just retry on the next tick.
    } finally {
      inFlight = false;
    }
  }

  subscribeBoothLog(() => {
    void ship();
  });
  setInterval(() => {
    void ship();
  }, INTERVAL_MS);
}
