import { describe, expect, it } from 'vitest';
import type { PeerEntry } from './types';
import { rosterSlots } from './roster_slots';

const basePeer = (overrides: Partial<PeerEntry>): PeerEntry => ({
  id: 'bob',
  instance_id: '',
  display_name: 'Bob',
  team: '',
  role: '',
  receive_status: 'ask_first',
  port_index: 2,
  online: true,
  lastSeenMs: Date.now(),
  ...overrides,
});

describe('rosterSlots', () => {
  it('returns three remote-leg slots when connected', () => {
    const slots = rosterSlots([], true, { display_name: 'Alice' }, 1);
    expect(slots).toHaveLength(3);
    expect(slots.map((slot) => slot.leg)).toEqual([0, 2, 3]);
    expect(slots.every((slot) => slot.peer === null)).toBe(true);
  });

  it('fills the slot matching an online peer leg', () => {
    const peer = basePeer({ id: 'bob-2', port_index: 2 });
    const slots = rosterSlots([peer], true, { display_name: 'Alice' }, 1);
    expect(slots.find((slot) => slot.leg === 2)?.peer?.display_name).toBe('Bob');
    expect(slots.filter((slot) => slot.peer).length).toBe(1);
  });

  it('returns empty when USB is not connected', () => {
    expect(rosterSlots([], false, { display_name: 'Alice' }, 1)).toEqual([]);
  });
});
