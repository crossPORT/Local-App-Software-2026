import { describe, expect, it, vi } from 'vitest';
import { sendAnnouncePresence, type AnnouncePresenceBag } from './announce_presence';
import type { SessionMessage } from '../session_types';
import type { SwitchDestCache } from './switch_cache';
import type { UsbEndpoints } from './usb_ids';

function msg(): SessionMessage {
  return {
    kind: 'announce',
    session_id: 's1',
    from_name: 'A',
    team: '',
    to_name: '',
    note: 'port=1',
    payload_type: '',
    payload_name: '',
    file_count: 0,
    total_bytes: 0,
  };
}

function makeBag(overrides: Partial<AnnouncePresenceBag> = {}) {
  const switched: number[] = [];
  const sent: SessionMessage[] = [];
  const switches = {
    last: null as number | null,
    preserve: false,
    markPreserve() {
      this.preserve = this.last != null && this.last >= 1 && this.last <= 4;
    },
    clear() {
      this.last = null;
      this.preserve = false;
    },
    async switchIfNeeded(_d: USBDevice, _e: UsbEndpoints, dest: number) {
      if (this.last === dest) return false;
      switched.push(dest);
      this.last = dest;
      if (dest === 0) this.preserve = false;
      return true;
    },
  };
  const bag: AnnouncePresenceBag = {
    device: {} as USBDevice,
    eps: { ep1Out: 2, ep2In: 1, ep4Out: 4 } as UsbEndpoints,
    portIndex: 0,
    switches: switches as unknown as SwitchDestCache,
    rotateIndex: { current: 0 },
    sendSession: async (m) => {
      sent.push(m);
    },
    ...overrides,
  };
  return { bag, switched, sent, switches };
}

describe('sendAnnouncePresence', () => {
  it('burst probes all remotes and leaves last dest (no clear)', async () => {
    const { bag, switched, sent, switches } = makeBag();
    await sendAnnouncePresence(bag, msg(), 'burst');
    expect(switched).toEqual([2, 3, 4]);
    expect(sent).toHaveLength(3);
    expect(switches.last).toBe(4);
  });

  it('rotate advances remotes when not sticky', async () => {
    const { bag, switched, sent, switches } = makeBag();
    await sendAnnouncePresence(bag, msg(), 'rotate');
    expect(switched).toEqual([2]);
    expect(sent).toHaveLength(1);
    expect(switches.last).toBe(2);
    await sendAnnouncePresence(bag, msg(), 'rotate');
    expect(switched).toEqual([2, 3]);
    expect(switches.last).toBe(3);
  });

  it('rotate while preserve holds without moving', async () => {
    const { bag, switched, sent, switches } = makeBag();
    switches.last = 2;
    switches.markPreserve();
    await sendAnnouncePresence(bag, msg(), 'rotate');
    expect(switched).toEqual([]);
    expect(sent).toHaveLength(1);
    expect(switches.last).toBe(2);
  });

  it('burst while preserve restores held dest', async () => {
    const { bag, switched, switches } = makeBag();
    switches.last = 2;
    switches.markPreserve();
    await sendAnnouncePresence(bag, msg(), 'burst');
    expect(switched).toEqual([3, 4, 2]);
    expect(switches.last).toBe(2);
  });

  it('skips redundant switch when dest unchanged', async () => {
    const switchIfNeeded = vi.fn(async (_d, _e, dest: number) => {
      void dest;
      return false;
    });
    const { bag, sent } = makeBag({
      switches: {
        switchIfNeeded,
        clear() {},
        markPreserve() {},
        last: null,
        preserve: false,
      } as unknown as SwitchDestCache,
    });
    await sendAnnouncePresence(bag, msg(), 'rotate');
    expect(switchIfNeeded).toHaveBeenCalledWith(bag.device, bag.eps, 2);
    expect(switchIfNeeded).not.toHaveBeenCalledWith(bag.device, bag.eps, 0);
    expect(sent).toHaveLength(1);
  });
});
