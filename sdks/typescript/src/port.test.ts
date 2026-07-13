import { describe, expect, it } from 'vitest';
import { RocketBoxError } from './errors';
import {
  PORT_COUNT,
  portIndexFromSerial,
  formatPortLabel,
  remotePortIndexes,
  remoteDisplayPorts,
  defaultPairDisplayPort,
  resolvePortIndexFromDevice,
} from './port';

describe('portIndexFromSerial', () => {
  it('maps RocketBox cable serials to port index mod 4', () => {
    expect(portIndexFromSerial('0000000000000001')).toBe(0);
    expect(portIndexFromSerial('0000000000000002')).toBe(1);
    expect(portIndexFromSerial('0000000000000003')).toBe(2);
    expect(portIndexFromSerial('0000000000000004')).toBe(3);
    expect(portIndexFromSerial('0000000000000005')).toBe(0);
  });

  it('throws for missing or invalid serials', () => {
    expect(() => portIndexFromSerial('')).toThrow(RocketBoxError);
    expect(() => portIndexFromSerial('not-hex')).toThrow(RocketBoxError);
  });
});

describe('resolvePortIndexFromDevice', () => {
  it('derives index only from device serial', () => {
    expect(resolvePortIndexFromDevice({ serialNumber: '0000000000000003' })).toBe(2);
  });

  it('throws when serial is missing', () => {
    expect(() => resolvePortIndexFromDevice({})).toThrow(RocketBoxError);
  });
});

describe('remotePortIndexes', () => {
  it('returns the three indexes that are not local', () => {
    expect(remotePortIndexes(1)).toEqual([0, 2, 3]);
  });
});

describe('PORT_COUNT', () => {
  it('is four', () => {
    expect(PORT_COUNT).toBe(4);
  });
});

describe('defaultPairDisplayPort', () => {
  it('pairs 1↔2 and 3↔4', () => {
    expect(defaultPairDisplayPort(1)).toBe(2);
    expect(defaultPairDisplayPort(2)).toBe(1);
    expect(defaultPairDisplayPort(3)).toBe(4);
    expect(defaultPairDisplayPort(4)).toBe(3);
  });

  it('throws for invalid display ports', () => {
    expect(() => defaultPairDisplayPort(0)).toThrow(RocketBoxError);
    expect(() => defaultPairDisplayPort(5)).toThrow(RocketBoxError);
  });
});

describe('remoteDisplayPorts', () => {
  it('returns the three remotes for port 1', () => {
    expect(remoteDisplayPorts(1)).toEqual([2, 3, 4]);
  });
});

describe('formatPortLabel', () => {
  it('shows 1-based port numbers', () => {
    expect(formatPortLabel(1, '0000000000000002')).toBe('Port 2');
    expect(formatPortLabel(0)).toBe('Port 1');
  });
});
