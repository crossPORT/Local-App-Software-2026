import { describe, expect, it } from 'vitest';
import { createFabricTransport } from '@rocketbox/sdk';

describe('RocketBox transport mixins', () => {
  it('createFabricTransport real returns connect/disconnect surface', () => {
    const t = createFabricTransport({ simulate: false });
    expect(typeof t.connect).toBe('function');
    expect(typeof t.disconnect).toBe('function');
    expect(typeof t.syncSystems).toBe('function');
    expect(typeof t.ensureCircuit).toBe('function');
    expect(typeof t.setListenMode).toBe('function');
  });

  it('createFabricTransport sim returns same FabricTransport surface', () => {
    const t = createFabricTransport({ simulate: true, port: 2 });
    expect(typeof t.connect).toBe('function');
    expect(typeof t.listSystems).toBe('function');
    expect(t.getFabricPortIndex()).toBe(1);
  });
});
