import { describe, expect, it, vi } from 'vitest';
import { fabricPortFromSerial, resolveFabricPortIndex } from './fabric_port';

describe('fabricPortFromSerial', () => {
  it('maps RocketBox cable serials to fabric ports', () => {
    expect(fabricPortFromSerial('0000000000000001')).toBe(0);
    expect(fabricPortFromSerial('0000000000000002')).toBe(1);
  });

  it('returns null for missing or invalid serials', () => {
    expect(fabricPortFromSerial(undefined)).toBeNull();
    expect(fabricPortFromSerial('')).toBeNull();
    expect(fabricPortFromSerial('not-hex')).toBeNull();
  });
});

describe('resolveFabricPortIndex', () => {
  it('uses serial when only one device is visible', () => {
    const device = { serialNumber: '0000000000000002' };
    expect(resolveFabricPortIndex(device, [device], 0)).toBe(1);
  });

  it('prefers serial over a stale saved port', () => {
    const storage = new Map<string, string>();
    vi.stubGlobal('sessionStorage', {
      getItem: (key: string) => storage.get(key) ?? null,
      setItem: (key: string, value: string) => {
        storage.set(key, value);
      },
      removeItem: (key: string) => {
        storage.delete(key);
      },
    });
    const serial = '0000000000000002';
    sessionStorage.setItem('rocketbox_fabric_port_' + serial, '0');
    const device = { serialNumber: serial };
    expect(resolveFabricPortIndex(device, [device], 0)).toBe(1);
    expect(sessionStorage.getItem('rocketbox_fabric_port_' + serial)).toBeNull();
    vi.unstubAllGlobals();
  });

  it('uses sort order when multiple devices are visible', () => {
    const low = { serialNumber: '0000000000000001' };
    const high = { serialNumber: '0000000000000002' };
    expect(resolveFabricPortIndex(low, [high, low], 0)).toBe(0);
    expect(resolveFabricPortIndex(high, [high, low], 0)).toBe(1);
  });
});
