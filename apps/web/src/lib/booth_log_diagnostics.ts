/**
 * Rule-based self-diagnosis for the device event log.
 *
 * The event log is a structured stream of `timestamp [port=N] event | detail`
 * lines. These heuristics turn recurring failure *signatures* into plain-English
 * "most likely cause + what to check" guidance, so an operator (or a future
 * debugging session) doesn't have to re-derive the same conclusion by hand.
 *
 * Rules are intentionally conservative: they require a strong signal (e.g. a run
 * of identical consecutive failures) before asserting a cause, and they phrase
 * findings as the likely cause rather than a certainty.
 */

const LOG_LINE_RE = /^(\S+)\s+\[port=([^\]]+)\]\s+(\S+)(?:\s+\|\s+(.*))?$/;

/** Consecutive failures of the same kind before we assert a hard cause. */
const ANNOUNCE_WEDGE_MIN = 3;
/** Incoming-frame failures within the window before we flag a flaky link. */
const FLAKY_RECEIVE_MIN = 3;

export type DiagnosisSeverity = 'error' | 'warn' | 'info';

export interface ParsedBoothLine {
  raw: string;
  time: string;
  /** Display port label as logged (usually "1".."4"). */
  port: string;
  event: string;
  detail: string;
}

export interface Diagnosis {
  /** Stable identifier, unique per (rule, port). */
  id: string;
  severity: DiagnosisSeverity;
  /** Display port number this concerns, or null for a fabric-wide finding. */
  port: number | null;
  title: string;
  /** What the operator should check to resolve it. */
  detail: string;
}

/** Parse one device log line. Returns null for meta lines (e.g. `[boot]`). */
export function parseBoothLogLine(line: string): ParsedBoothLine | null {
  const match = LOG_LINE_RE.exec(line);
  if (!match) {
    return null;
  }
  const [, time, port, event, detail = ''] = match;
  return { raw: line, time, port, event, detail };
}

const SEVERITY_RANK: Record<DiagnosisSeverity, number> = {
  error: 0,
  warn: 1,
  info: 2,
};

function lastIndexWhere(
  entries: ParsedBoothLine[],
  predicate: (entry: ParsedBoothLine) => boolean,
): number {
  for (let i = entries.length - 1; i >= 0; i -= 1) {
    if (predicate(entries[i])) {
      return i;
    }
  }
  return -1;
}

function diagnosePort(
  portLabel: string,
  portNum: number | null,
  entries: ParsedBoothLine[],
): Diagnosis[] {
  const out: Diagnosis[] = [];
  const label = portNum !== null ? `Port ${portNum}` : `Port ${portLabel}`;

  // Rule A — announce wedge: a run of announce_fail timeouts since the last
  // successful send means the OUT endpoint is not being drained by any peer.
  let timeoutStreak = 0;
  let otherFailStreak = 0;
  let lastOtherFailDetail = '';
  for (const entry of entries) {
    if (entry.event === 'announce_sent') {
      timeoutStreak = 0;
      otherFailStreak = 0;
      lastOtherFailDetail = '';
    } else if (entry.event === 'announce_fail') {
      if (/transferout timed out/i.test(entry.detail)) {
        timeoutStreak += 1;
      } else {
        otherFailStreak += 1;
        lastOtherFailDetail = entry.detail;
      }
    }
  }
  if (timeoutStreak >= ANNOUNCE_WEDGE_MIN) {
    out.push({
      id: `announce_wedge:${portLabel}`,
      severity: 'error',
      port: portNum,
      title: `${label}: announces aren't leaving this device`,
      detail:
        `${timeoutStreak} announces in a row timed out on the USB OUT endpoint — ` +
        'nothing on the fabric is draining the data. Check that another RocketBox ' +
        'is connected with its app open and listening, then re-seat the cable if it persists.',
    });
  } else if (otherFailStreak >= ANNOUNCE_WEDGE_MIN) {
    out.push({
      id: `announce_fail:${portLabel}`,
      severity: 'warn',
      port: portNum,
      title: `${label}: announces are failing`,
      detail:
        `${otherFailStreak} announces failed to send` +
        (lastOtherFailDetail ? ` (last error: ${lastOtherFailDetail})` : '') +
        '. The link may be unstable — check the cable and the partner device.',
    });
  }

  // Rule B/C — current skip reason: only when it is the latest announce-related
  // state (no successful send after it) and the reason is actionable.
  const lastSkipIdx = lastIndexWhere(entries, (e) => e.event === 'announce_skip');
  const lastSentIdx = lastIndexWhere(entries, (e) => e.event === 'announce_sent');
  if (lastSkipIdx > lastSentIdx) {
    const reason = entries[lastSkipIdx].detail;
    if (reason === 'usb disconnected') {
      out.push({
        id: `usb_disconnected:${portLabel}`,
        severity: 'warn',
        port: portNum,
        title: `${label}: USB disconnected`,
        detail: 'The RocketBox cable is not connected — reconnect it to resume announcing.',
      });
    } else if (reason === 'display_name not set') {
      out.push({
        id: `no_display_name:${portLabel}`,
        severity: 'warn',
        port: portNum,
        title: `${label}: not announcing — no display name`,
        detail: 'Open Settings and set a display name so this device can announce itself to peers.',
      });
    }
  }

  // Rule D — flaky receive: repeated incoming-frame failures mean a peer is
  // sending but transfers aren't completing.
  const recvFails = entries.filter(
    (e) => e.event === 'session_body_timeout' || e.event === 'usb_recv_fail',
  ).length;
  if (recvFails >= FLAKY_RECEIVE_MIN) {
    out.push({
      id: `flaky_receive:${portLabel}`,
      severity: 'warn',
      port: portNum,
      title: `${label}: incoming frames aren't completing`,
      detail:
        `${recvFails} incoming transfers timed out or failed mid-frame. The partner is ` +
        'sending but the link is unstable, or the partner stopped mid-send.',
    });
  }

  return out;
}

function sortDiagnoses(diagnoses: Diagnosis[]): Diagnosis[] {
  return [...diagnoses].sort((a, b) => {
    const bySeverity = SEVERITY_RANK[a.severity] - SEVERITY_RANK[b.severity];
    if (bySeverity !== 0) {
      return bySeverity;
    }
    return (a.port ?? Number.MAX_SAFE_INTEGER) - (b.port ?? Number.MAX_SAFE_INTEGER);
  });
}

/**
 * Inspect a window of device log lines and surface likely issues, most severe
 * first. Returns an empty array when nothing matches a known failure signature.
 */
export function diagnoseBoothLog(lines: string[]): Diagnosis[] {
  const parsed = lines
    .map(parseBoothLogLine)
    .filter((entry): entry is ParsedBoothLine => entry !== null);

  const byPort = new Map<string, ParsedBoothLine[]>();
  for (const entry of parsed) {
    const list = byPort.get(entry.port);
    if (list) {
      list.push(entry);
    } else {
      byPort.set(entry.port, [entry]);
    }
  }

  const diagnoses: Diagnosis[] = [];
  for (const [portLabel, entries] of byPort) {
    const portNum = /^\d+$/.test(portLabel) ? Number.parseInt(portLabel, 10) : null;
    diagnoses.push(...diagnosePort(portLabel, portNum, entries));
  }

  // Fabric-wide — no peers: announcing but never heard a peer announce back.
  // Suppressed when a more specific announce failure already explains the
  // silence (e.g. a wedge means our announces never went out).
  const anyAttempt = parsed.some(
    (e) => e.event === 'announce_attempt' || e.event === 'announce_sent',
  );
  const anyReceived = parsed.some((e) => e.event === 'announce_received');
  const anyAnnounceProblem = diagnoses.some(
    (d) => d.id.startsWith('announce_wedge:') || d.id.startsWith('announce_fail:'),
  );
  if (anyAttempt && !anyReceived && !anyAnnounceProblem) {
    diagnoses.push({
      id: 'no_peers',
      severity: 'info',
      port: null,
      title: 'No peers seen yet',
      detail:
        'This device is announcing but no peer announce has been received. Confirm at ' +
        'least one other RocketBox is connected and running its app.',
    });
  }

  return sortDiagnoses(diagnoses);
}
