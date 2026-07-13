import { describe, expect, it } from 'vitest';
import { createRocketBoxTransport } from '@rocketbox/sdk';

describe('RocketBox transport mixins', () => {
  it('createRocketBoxTransport real returns connect/disconnect surface', () => {
    const t = createRocketBoxTransport({ simulate: false });
    expect(typeof t.connect).toBe('function');
    expect(typeof t.disconnect).toBe('function');
    expect(typeof t.syncSystems).toBe('function');
    expect(typeof t.ensureCircuit).toBe('function');
    expect(typeof t.setListenMode).toBe('function');
  });

  it('createRocketBoxTransport sim returns same RocketBoxTransport surface', () => {
    const t = createRocketBoxTransport({ simulate: true, port: 2 });
    expect(typeof t.connect).toBe('function');
    expect(typeof t.listSystems).toBe('function');
    expect(t.getPortIndex()).toBe(1);
  });
});
