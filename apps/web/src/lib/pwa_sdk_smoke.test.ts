/**
 * Smoke: PWA-shaped flow over createRocketBoxTransport (sim).
 * Requires simulated-hardware on :1773 for full path; otherwise skipped soft.
 */
import { describe, expect, it } from 'vitest';
import { createRocketBoxTransport } from '@rocketbox/sdk';

describe('PWA SDK smoke', () => {
  it('createRocketBoxTransport sim exposes link/session/data surface', () => {
    const t = createRocketBoxTransport({ simulate: true, port: 1 });
    expect(typeof t.connect).toBe('function');
    expect(typeof t.ensureCircuit).toBe('function');
    expect(typeof t.sendBytes).toBe('function');
    expect(typeof t.clearCircuit).toBe('function');
  });
});
