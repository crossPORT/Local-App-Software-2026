import { toDisplayPort } from '@rocketbox/sdk';
import { useEffect, useState } from 'react';
import { peerRosterLabel } from '../lib/format';
import { rosterSlots } from '../lib/roster_slots';
import { theme } from '../lib/theme';
import type { LinkUiState, PeerEntry } from '../lib/types';
import { PeerRow } from './PeerRow';

interface RosterPanelProps {
  peers: PeerEntry[];
  usbConnected: boolean;
  identityConfigured: boolean;
  identityDisplayName: string;
  localLeg: number;
  busy: boolean;
  statusMessage: string;
  selectedPeer: string;
  lastAnnounceMs: number;
  linkedPort?: number;
  linkState?: LinkUiState;
  announceIntervalSec?: number;
  onSelectPeer: (peerId: string) => void;
  onDropFiles: (peerId: string, files: File[]) => void | Promise<void>;
  onOpenSettings?: () => void;
  onDropError?: (message: string) => void;
  onRequestReleaseLink?: (peer: PeerEntry) => void;
}

function rosterEmptyMessage(usbConnected: boolean, identityConfigured: boolean): string {
  if (!usbConnected) {
    return identityConfigured
      ? 'Connect USB to discover other stations'
      : 'Set your name in Settings, then connect USB';
  }
  return '';
}

function useTickSeconds(): number {
  const [tick, setTick] = useState(() => Date.now());
  useEffect(() => {
    const id = window.setInterval(() => setTick(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, []);
  return tick;
}

export function RosterPanel({
  peers,
  usbConnected,
  identityConfigured,
  identityDisplayName,
  localLeg,
  busy,
  statusMessage,
  selectedPeer,
  lastAnnounceMs,
  linkedPort = 0,
  linkState = 'none',
  announceIntervalSec = 10,
  onSelectPeer,
  onDropFiles,
  onOpenSettings,
  onDropError,
  onRequestReleaseLink,
}: RosterPanelProps) {
  const now = useTickSeconds();
  const slots = rosterSlots(
    peers,
    usbConnected,
    { display_name: identityDisplayName },
    localLeg,
  );
  const onlinePeers = slots.flatMap((slot) => (slot.peer ? [slot.peer] : []));
  const empty = rosterEmptyMessage(usbConnected, identityConfigured);
  const intervalMs = (announceIntervalSec ?? 10) * 1000;
  const nextAnnounceIn =
    lastAnnounceMs > 0 ? Math.max(0, intervalMs - (now - lastAnnounceMs)) : 0;
  const announceStalled = lastAnnounceMs > 0 && now - lastAnnounceMs > intervalMs * 2;

  return (
    <section className="roster panel-inner">
      <div
        className="section-label"
        style={{ color: theme.accent, display: 'flex', justifyContent: 'space-between' }}
      >
        <span>Connected peers</span>
        {usbConnected && lastAnnounceMs > 0 && (
          <span
            style={{
              fontSize: '0.75rem',
              color: announceStalled ? theme.warn : theme.muted,
              fontWeight: 400,
            }}
          >
            {announceStalled
              ? 'Discovery stalled — reconnect'
              : `Next sync in ${Math.ceil(nextAnnounceIn / 1000)}s`}
          </span>
        )}
      </div>
      <div className="peers-container">
        {slots.length === 0 ? (
          <div className="roster-empty">
            <p className="empty-hint" style={{ color: theme.muted }}>
              {empty}
            </p>
            {!identityConfigured && onOpenSettings && (
              <button type="button" className="settings-inline-btn" onClick={onOpenSettings}>
                Open Settings
              </button>
            )}
          </div>
        ) : (
          slots.map((slot) => {
            if (slot.peer) {
              const port = toDisplayPort(slot.peer.port_index);
              const rowLink: LinkUiState =
                linkState !== 'none' && linkedPort === port ? linkState : 'none';
              return (
                <PeerRow
                  key={`leg-${slot.leg}`}
                  peer={slot.peer}
                  label={peerRosterLabel(slot.peer, onlinePeers)}
                  offline={false}
                  selected={selectedPeer === slot.peer.id}
                  busy={busy && selectedPeer === slot.peer.id}
                  linkState={rowLink}
                  statusMessage={statusMessage}
                  now={now}
                  onSelect={() => onSelectPeer(slot.peer!.id)}
                  onFiles={(files) => onDropFiles(slot.peer!.id, files)}
                  onDropError={onDropError}
                  onReleaseLink={
                    rowLink !== 'none' && onRequestReleaseLink
                      ? () => onRequestReleaseLink(slot.peer!)
                      : undefined
                  }
                />
              );
            }
            return (
              <PeerRow
                key={`leg-${slot.leg}`}
                peer={null}
                label={`Port ${toDisplayPort(slot.leg)}`}
                offline
                leg={slot.leg}
                selected={false}
                busy={false}
                linkState="none"
                statusMessage=""
                now={now}
                onSelect={() => {}}
                onFiles={() => {}}
              />
            );
          })
        )}
      </div>
    </section>
  );
}
