import { describe, expect, it, vi } from 'vitest';
import { PeerRoster } from './peer_roster';

describe('PeerRoster', () => {
  it('seeds configured peers as offline bookmarks', () => {
    const roster = new PeerRoster();
    roster.seedFromConfig([
      {
        display_name: 'Bob',
        team: 'Creative',
        role: '',
        receive_status: 'open',
        port_index: 1,
      },
    ]);
    expect(roster.hasConfiguredPeers()).toBe(true);
    expect(roster.visiblePeers(true)).toHaveLength(0);
    expect(roster.findByName('Bob')?.online).toBe(false);
  });

  it('marks a peer online on announce touch', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Alice', 'CAD', 'ask_first', 0);
    const visible = roster.visiblePeers(true);
    expect(visible).toHaveLength(1);
    expect(visible[0]?.display_name).toBe('Alice');
    expect(visible[0]?.online).toBe(true);
  });

  it('replaces peer on the same port when name changes', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Old-Name', 'CAD', 'open', 1);
    roster.touchPeer('New-Name', 'CAD', 'open', 1);
    expect(roster.findByPort(1)?.display_name).toBe('New-Name');
    expect(roster.findByName('Old-Name')).toBeUndefined();
    expect(roster.visiblePeers(true)).toHaveLength(1);
  });

  it('marks stale peers offline', () => {
    vi.useFakeTimers();
    const roster = new PeerRoster();
    roster.touchPeer('Alice', 'CAD', 'open', 0);
    vi.setSystemTime(Date.now() + 46_000);
    roster.markStalePeersOffline();
    expect(roster.hasOnlinePeers()).toBe(false);
    vi.useRealTimers();
  });

  it('hides roster when fabric is disconnected', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Alice', 'CAD', 'open', 0);
    expect(roster.visiblePeers(false)).toEqual([]);
  });

  it('sorts visible peers by display name', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Zed', 'T', 'open', 2);
    roster.touchPeer('Amy', 'T', 'open', 1);
    expect(roster.visiblePeers(true).map((peer) => peer.display_name)).toEqual(['Amy', 'Zed']);
  });
});
