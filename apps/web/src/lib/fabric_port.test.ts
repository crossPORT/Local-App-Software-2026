import { describe, expect, it } from 'vitest';
import { FabricUsbError } from './fabric_errors';
import {
  FABRIC_LEG_COUNT,
  fabricLegFromSerial,
  fabricPortFromSerial,
  formatFabricPortDisplay,
  remoteFabricLegs,
  resolveFabricLegFromDevice,
} from './fabric_port';

describe('fabricLegFromSerial', () => {
  it('maps RocketBox cable serials to fabric legs mod 4', () => {
    expect(fabricLegFromSerial('0000000000000001')).toBe(0);
    expect(fabricLegFromSerial('0000000000000002')).toBe(1);
    expect(fabricLegFromSerial('0000000000000003')).toBe(2);
    expect(fabricLegFromSerial('0000000000000004')).toBe(3);
    expect(fabricLegFromSerial('0000000000000005')).toBe(0);
  });

  it('throws for missing or invalid serials', () => {
    expect(() => fabricLegFromSerial('')).toThrow(FabricUsbError);
    expect(() => fabricLegFromSerial('not-hex')).toThrow(FabricUsbError);
  });
});

describe('fabricPortFromSerial', () => {
  it('returns null for invalid serials', () => {
    expect(fabricPortFromSerial(undefined)).toBeNull();
    expect(fabricPortFromSerial('')).toBeNull();
    expect(fabricPortFromSerial('not-hex')).toBeNull();
  });
});

describe('resolveFabricLegFromDevice', () => {
  it('derives leg only from device serial', () => {
    expect(resolveFabricLegFromDevice({ serialNumber: '0000000000000003' })).toBe(2);
  });

  it('throws when serial is missing', () => {
    expect(() => resolveFabricLegFromDevice({})).toThrow(FabricUsbError);
  });
});

describe('remoteFabricLegs', () => {
  it('returns the three legs that are not local', () => {
    expect(remoteFabricLegs(1)).toEqual([0, 2, 3]);
  });
});

describe('FABRIC_LEG_COUNT', () => {
  it('is four', () => {
    expect(FABRIC_LEG_COUNT).toBe(4);
  });
});

describe('formatFabricPortDisplay', () => {
  it('shows 1-based port numbers', () => {
    expect(formatFabricPortDisplay(1, '0000000000000002')).toBe('Port 2');
    expect(formatFabricPortDisplay(0)).toBe('Port 1');
  });
});
