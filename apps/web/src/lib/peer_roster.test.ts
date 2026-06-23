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
    expect(visible[0]?.id).toBe('0:Alice');
    expect(visible[0]?.online).toBe(true);
  });

  it('shows one online peer per fabric port', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Alice', 'Creative', 'open', 1, 'new-session');
    roster.touchPeer('Alice', 'Creative', 'ask_first', 1);
    expect(roster.visiblePeers(true)).toHaveLength(1);
    expect(roster.visiblePeers(true)[0]?.receive_status).toBe('ask_first');
  });

  it('keeps duplicate display names on different fabric ports', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Sally', 'Creative', 'open', 1, 'phone');
    roster.touchPeer('Sally', 'CAD', 'ask_first', 0, 'laptop');
    const visible = roster.visiblePeers(true);
    expect(visible).toHaveLength(2);
    expect(visible.map((peer) => peer.id).sort()).toEqual(['i:laptop', 'i:phone']);
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

  it('keeps a freshly-seen peer online across a transient disconnect (markStale)', () => {
    // A brief auto-recover reconnect must not wipe presence: only setAllPeersOffline
    // (manual disconnect) should drop a fresh peer; markStalePeersOffline keeps it
    // so it reappears instantly on reconnect instead of waiting for a re-announce.
    const fresh = new PeerRoster();
    fresh.touchPeer('Alice', 'CAD', 'open', 0);
    fresh.markStalePeersOffline();
    expect(fresh.visiblePeers(true)).toHaveLength(1);

    const manual = new PeerRoster();
    manual.touchPeer('Alice', 'CAD', 'open', 0);
    manual.setAllPeersOffline();
    expect(manual.visiblePeers(true)).toHaveLength(0);
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

  it('tracks three online remotes on legs 1–3', () => {
    const roster = new PeerRoster();
    roster.touchPeer('Booth-A', 'T', 'open', 1);
    roster.touchPeer('Booth-B', 'T', 'ask_first', 2);
    roster.touchPeer('Booth-C', 'T', 'open', 3);
    const visible = roster.visiblePeers(true);
    expect(visible).toHaveLength(3);
    expect(visible.map((peer) => peer.port_index).sort()).toEqual([1, 2, 3]);
  });
});
