import type { PeerConfig, PeerEntry, ReceiveStatus } from './types';

export const DEFAULT_STALE_MS = 45_000;

interface TrackedPeer extends PeerEntry {}

function peerKey(portIndex: number, displayName: string): string {
  return `${portIndex}:${displayName}`;
}

export class PeerRoster {
  private peers = new Map<string, TrackedPeer>();

  seedFromConfig(configured: PeerConfig[]): void {
    const next = new Map<string, TrackedPeer>();
    for (const peer of configured) {
      const display_name = peer.display_name.trim();
      if (!display_name) {
        continue;
      }
      const key = peerKey(peer.port_index, display_name);
      const existing = this.peers.get(key);
      next.set(key, {
        ...peer,
        display_name,
        online: false,
        lastSeenMs: existing?.lastSeenMs ?? 0,
      });
    }
    for (const [key, peer] of this.peers) {
      if (!next.has(key) && peer.online) {
        next.set(key, { ...peer });
      }
    }
    this.peers = next;
  }

  touchPeer(
    displayName: string,
    team: string,
    receiveStatus: ReceiveStatus,
    portIndex: number,
  ): void {
    const display_name = displayName.trim();
    if (!display_name) {
      return;
    }
    const now = Date.now();

    // Announce is source of truth: one live peer per USB port.
    for (const [key, peer] of this.peers) {
      if (peer.port_index === portIndex) {
        if (peer.display_name !== display_name) {
          this.peers.delete(key);
        }
        this.peers.set(peerKey(portIndex, display_name), {
          ...peer,
          display_name,
          team: team.trim() || peer.team,
          receive_status: receiveStatus,
          port_index: portIndex,
          online: true,
          lastSeenMs: now,
        });
        return;
      }
    }

    for (const peer of this.peers.values()) {
      if (peer.display_name === display_name) {
        peer.online = true;
        peer.lastSeenMs = now;
        if (team.trim()) {
          peer.team = team.trim();
        }
        peer.receive_status = receiveStatus;
        peer.port_index = portIndex;
        return;
      }
    }

    this.peers.set(peerKey(portIndex, display_name), {
      display_name,
      team: team.trim(),
      role: '',
      receive_status: receiveStatus,
      port_index: portIndex,
      online: true,
      lastSeenMs: now,
    });
  }

  touchPeerPresence(displayName: string, team: string): void {
    const display_name = displayName.trim();
    if (!display_name) {
      return;
    }
    for (const peer of this.peers.values()) {
      if (peer.display_name === display_name) {
        peer.online = true;
        peer.lastSeenMs = Date.now();
        if (team.trim()) {
          peer.team = team.trim();
        }
        return;
      }
    }
  }

  markStalePeersOffline(maxAgeMs = DEFAULT_STALE_MS): void {
    const now = Date.now();
    for (const peer of this.peers.values()) {
      if (!peer.online || peer.lastSeenMs === 0) {
        peer.online = false;
        continue;
      }
      if (now - peer.lastSeenMs > maxAgeMs) {
        peer.online = false;
      }
    }
  }

  setAllPeersOffline(): void {
    for (const peer of this.peers.values()) {
      peer.online = false;
    }
  }

  findByName(displayName: string): PeerEntry | undefined {
    for (const peer of this.peers.values()) {
      if (peer.display_name === displayName) {
        return peer;
      }
    }
    return undefined;
  }

  findByPort(portIndex: number): PeerEntry | undefined {
    for (const peer of this.peers.values()) {
      if (peer.port_index === portIndex) {
        return peer;
      }
    }
    return undefined;
  }

  visiblePeers(fabricConnected: boolean): PeerEntry[] {
    if (!fabricConnected) {
      return [];
    }
    return [...this.peers.values()]
      .filter((peer) => peer.online)
      .sort((a, b) => a.display_name.localeCompare(b.display_name));
  }

  hasConfiguredPeers(): boolean {
    return this.peers.size > 0;
  }

  hasOnlinePeers(): boolean {
    return [...this.peers.values()].some((peer) => peer.online);
  }
}
