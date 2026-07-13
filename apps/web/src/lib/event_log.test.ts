import { describe, expect, it } from 'vitest';
import {
  eventLog,
  clearBoothLog,
  filterBoothLogLinesByPort,
  getEventLogLevel,
  readBoothLogTail,
  setEventLogLevel,
  subscribeEventLog,
} from './event_log';

describe('event_log', () => {
  it('does not append when level is off', () => {
    setEventLogLevel('off');
    clearBoothLog();
    eventLog(0, 'test_event', 'detail');
    expect(readBoothLogTail()).toContain('log cleared');
    expect(readBoothLogTail()).not.toContain('test_event');
  });

  it('appends lines in native-like format', () => {
    setEventLogLevel('normal');
    clearBoothLog();
    eventLog(2, 'link_activity', 'idle→handshake');
    const tail = readBoothLogTail(10);
    expect(tail).toMatch(
      /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z \[port=3\] link_activity \| idle→handshake/m,
    );
    setEventLogLevel('off');
  });

  it('evicts oldest lines beyond cap', () => {
    setEventLogLevel('normal');
    clearBoothLog();
    for (let i = 0; i < 405; i += 1) {
      eventLog(0, 'line', String(i));
    }
    const tail = readBoothLogTail(500);
    expect(tail).not.toContain('line | 0');
    expect(tail).toContain('line | 404');
    setEventLogLevel('off');
  });

  it('reports current level', () => {
    setEventLogLevel('verbose');
    expect(getEventLogLevel()).toBe('verbose');
    setEventLogLevel('off');
  });

  it('notifies subscribers when lines change', () => {
    setEventLogLevel('normal');
    clearBoothLog();
    let count = 0;
    const unsub = subscribeEventLog(() => {
      count += 1;
    });
    eventLog(1, 'usb_connect', 'test');
    clearBoothLog();
    unsub();
    expect(count).toBe(2);
    setEventLogLevel('off');
  });

  it('filters lines by fabric port', () => {
    const lines = [
      '2026-01-01T00:00:00.000Z [port=1] announce_sent | A',
      '2026-01-01T00:00:01.000Z [port=2] announce_sent | B',
      '2026-01-01T00:00:02.000Z [boot] log cleared',
    ];
    expect(filterBoothLogLinesByPort(lines, 1)).toEqual([
      '2026-01-01T00:00:01.000Z [port=2] announce_sent | B',
      '2026-01-01T00:00:02.000Z [boot] log cleared',
    ]);
  });
});
