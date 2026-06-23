import type { PeerConfig, PeerEntry, ReceiveStatus } from './types';

export const DEFAULT_STALE_MS = 45_000;

interface TrackedPeer extends PeerEntry {}

export function peerRosterKey(portIndex: number, displayName: string, instanceId = ''): string {
  if (instanceId) {
    return `i:${instanceId}`;
  }
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
      const key = peerRosterKey(peer.port_index, display_name);
      const existing = this.peers.get(key);
      next.set(key, {
        ...peer,
        id: key,
        instance_id: '',
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
    instanceId = '',
  ): void {
    const display_name = displayName.trim();
    if (!display_name) {
      return;
    }
    const now = Date.now();
    const instance_id = instanceId.trim();
    const id = peerRosterKey(portIndex, display_name, instance_id);

    // One live station per fabric port — drop stale bookmarks/legacy rows.
    for (const [key, peer] of this.peers) {
      if (peer.port_index === portIndex && key !== id) {
        this.peers.delete(key);
      }
    }

    if (instance_id) {
      const existing = this.peers.get(id);
      this.peers.set(id, {
        ...(existing ?? {
          display_name,
          team: '',
          role: '',
          receive_status: receiveStatus,
          port_index: portIndex,
        }),
        id,
        instance_id,
        display_name,
        team: team.trim() || existing?.team || '',
        receive_status: receiveStatus,
        port_index: portIndex,
        online: true,
        lastSeenMs: now,
      });
      return;
    }

    // Announce is source of truth: one live peer per USB port (legacy announces without instance).
    for (const [key, peer] of this.peers) {
      if (peer.port_index === portIndex) {
        if (peer.display_name !== display_name) {
          this.peers.delete(key);
        }
        this.peers.set(peerRosterKey(portIndex, display_name), {
          ...peer,
          id: peerRosterKey(portIndex, display_name),
          instance_id: '',
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
      if (peer.display_name === display_name && !peer.instance_id) {
        peer.online = true;
        peer.lastSeenMs = now;
        if (team.trim()) {
          peer.team = team.trim();
        }
        peer.receive_status = receiveStatus;
        peer.port_index = portIndex;
        peer.id = peerRosterKey(portIndex, display_name);
        return;
      }
    }

    this.peers.set(id, {
      id,
      instance_id: '',
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

  findById(peerId: string): PeerEntry | undefined {
    return this.peers.get(peerId);
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
    const byPort = new Map<number, PeerEntry>();
    for (const peer of this.peers.values()) {
      if (!peer.online) {
        continue;
      }
      const existing = byPort.get(peer.port_index);
      if (!existing || peer.lastSeenMs > existing.lastSeenMs) {
        byPort.set(peer.port_index, peer);
      }
    }
    return [...byPort.values()].sort((a, b) => a.display_name.localeCompare(b.display_name));
  }

  hasConfiguredPeers(): boolean {
    return this.peers.size > 0;
  }

  hasOnlinePeers(): boolean {
    return [...this.peers.values()].some((peer) => peer.online);
  }
}
