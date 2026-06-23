import { describe, expect, it } from 'vitest';
import {
  diagnoseBoothLog,
  parseBoothLogLine,
} from './booth_log_diagnostics';

const T = '2026-06-23T16:24:51.100Z';

function line(port: number | 'boot', event: string, detail = ''): string {
  const tag = port === 'boot' ? '[boot]' : `[port=${port}]`;
  return detail ? `${T} ${tag} ${event} | ${detail}` : `${T} ${tag} ${event}`;
}

describe('parseBoothLogLine', () => {
  it('parses a structured line', () => {
    const parsed = parseBoothLogLine(`${T} [port=2] announce_fail | transferOut timed out after 2500ms`);
    expect(parsed).toEqual({
      raw: `${T} [port=2] announce_fail | transferOut timed out after 2500ms`,
      time: T,
      port: '2',
      event: 'announce_fail',
      detail: 'transferOut timed out after 2500ms',
    });
  });

  it('returns null for meta lines', () => {
    expect(parseBoothLogLine(`${T} [boot] log cleared`)).toBeNull();
  });
});

describe('diagnoseBoothLog', () => {
  it('returns nothing for a clean log', () => {
    const lines = [
      line(2, 'announce_attempt', 'scheduled'),
      line(2, 'announce_sent', 'Alice'),
      line(2, 'announce_received', 'Bob port=1'),
    ];
    expect(diagnoseBoothLog(lines)).toEqual([]);
  });

  it('flags an announce wedge after consecutive transferOut timeouts', () => {
    const lines: string[] = [];
    for (let i = 0; i < 3; i += 1) {
      lines.push(line(2, 'announce_attempt', 'scheduled'));
      lines.push(line(2, 'session_send_prep', 'ready'));
      lines.push(line(2, 'announce_fail', 'transferOut timed out after 2500ms'));
    }
    const result = diagnoseBoothLog(lines);
    const wedge = result.find((d) => d.id === 'announce_wedge:2');
    expect(wedge).toBeDefined();
    expect(wedge?.severity).toBe('error');
    expect(wedge?.port).toBe(2);
    expect(wedge?.title).toContain('announces aren');
    // The fabric-wide "no peers" info is suppressed by the specific wedge cause.
    expect(result.some((d) => d.id === 'no_peers')).toBe(false);
  });

  it('does not flag a wedge below the threshold', () => {
    const lines = [
      line(2, 'announce_fail', 'transferOut timed out after 2500ms'),
      line(2, 'announce_fail', 'transferOut timed out after 2500ms'),
    ];
    expect(diagnoseBoothLog(lines).some((d) => d.id.startsWith('announce_wedge'))).toBe(false);
  });

  it('resets the wedge streak after a successful send', () => {
    const lines = [
      line(2, 'announce_fail', 'transferOut timed out after 2500ms'),
      line(2, 'announce_fail', 'transferOut timed out after 2500ms'),
      line(2, 'announce_sent', 'Alice'),
      line(2, 'announce_fail', 'transferOut timed out after 2500ms'),
    ];
    expect(diagnoseBoothLog(lines).some((d) => d.id.startsWith('announce_wedge'))).toBe(false);
  });

  it('flags a missing display name when it is the current skip reason', () => {
    const lines = [
      line(1, 'announce_skip', 'display_name not set'),
      line(1, 'announce_skip', 'display_name not set'),
    ];
    const result = diagnoseBoothLog(lines);
    const finding = result.find((d) => d.id === 'no_display_name:1');
    expect(finding).toBeDefined();
    expect(finding?.severity).toBe('warn');
  });

  it('flags usb disconnected as the current skip reason', () => {
    const lines = [line(3, 'announce_skip', 'usb disconnected')];
    expect(diagnoseBoothLog(lines).some((d) => d.id === 'usb_disconnected:3')).toBe(true);
  });

  it('ignores transient skip reasons like busy', () => {
    const lines = [line(2, 'announce_skip', 'busy'), line(2, 'announce_skip', 'pending_inbound')];
    expect(diagnoseBoothLog(lines)).toEqual([]);
  });

  it('flags a flaky receive link after repeated frame failures', () => {
    const lines = [
      line(2, 'session_body_timeout', '4096/20000'),
      line(2, 'usb_recv_fail', 'transfer error'),
      line(2, 'session_body_timeout', '8192/20000'),
    ];
    const finding = diagnoseBoothLog(lines).find((d) => d.id === 'flaky_receive:2');
    expect(finding).toBeDefined();
    expect(finding?.severity).toBe('warn');
  });

  it('reports no peers when announcing succeeds but none are heard', () => {
    const lines = [
      line(2, 'announce_attempt', 'scheduled'),
      line(2, 'announce_sent', 'Alice'),
    ];
    const result = diagnoseBoothLog(lines);
    expect(result).toHaveLength(1);
    expect(result[0]).toMatchObject({ id: 'no_peers', severity: 'info', port: null });
  });

  it('orders findings by severity', () => {
    const lines = [
      line(2, 'announce_attempt', 'scheduled'),
      line(1, 'announce_skip', 'usb disconnected'),
    ];
    for (let i = 0; i < 3; i += 1) {
      lines.push(line(2, 'announce_fail', 'transferOut timed out after 2500ms'));
    }
    const result = diagnoseBoothLog(lines);
    expect(result[0].severity).toBe('error');
  });
});
