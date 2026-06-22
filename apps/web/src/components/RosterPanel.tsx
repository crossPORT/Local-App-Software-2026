import { useEffect, useRef, useState } from 'react';
import { collectDropFiles } from '../lib/collect_drop_files';
import { isOutboundHandshakeWait, receiveStatusLabel } from '../lib/format';
import { DEFAULT_STALE_MS } from '../lib/peer_roster';
import { theme } from '../lib/theme';
import type { PeerEntry } from '../lib/types';
import { ANNOUNCE_INTERVAL_MS } from '../lib/web_transfer_orchestrator';

interface RosterPanelProps {
  peers: PeerEntry[];
  fabricConnected: boolean;
  identityConfigured: boolean;
  busy: boolean;
  statusMessage: string;
  selectedPeer: string;
  lastAnnounceMs: number;
  onSelectPeer: (name: string) => void;
  onDropFiles: (peerName: string, files: File[]) => void | Promise<void>;
  onOpenSettings?: () => void;
  onDropError?: (message: string) => void;
}

function rosterEmptyMessage(
  fabricConnected: boolean,
  identityConfigured: boolean,
): string {
  if (!fabricConnected) {
    return identityConfigured
      ? 'Connect USB to discover other stations'
      : 'Set your name in Settings, then connect USB';
  }
  return 'Listening for other stations — make sure the other app is connected too';
}

function useTickSeconds(): number {
  const [tick, setTick] = useState(() => Date.now());
  useEffect(() => {
    const id = window.setInterval(() => setTick(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, []);
  return tick;
}

function formatSecondsLeft(ms: number): string {
  const s = Math.max(0, Math.ceil(ms / 1000));
  return `${s}s`;
}

export function RosterPanel({
  peers,
  fabricConnected,
  identityConfigured,
  busy,
  statusMessage,
  selectedPeer,
  lastAnnounceMs,
  onSelectPeer,
  onDropFiles,
  onOpenSettings,
  onDropError,
}: RosterPanelProps) {
  const now = useTickSeconds();
  const visible = fabricConnected ? peers.filter((p) => p.online) : [];
  const empty = rosterEmptyMessage(fabricConnected, identityConfigured);
  const showSettingsAction = !identityConfigured && onOpenSettings;

  const nextAnnounceIn = lastAnnounceMs > 0
    ? Math.max(0, ANNOUNCE_INTERVAL_MS - (now - lastAnnounceMs))
    : 0;

  return (
    <section className="roster panel-inner">
      <div className="section-label" style={{ color: theme.accent, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <span>Connected peers</span>
        {fabricConnected && lastAnnounceMs > 0 && (
          <span style={{ fontSize: '0.75rem', color: theme.muted, fontWeight: 400 }}>
            Next announce in {formatSecondsLeft(nextAnnounceIn)}
          </span>
        )}
      </div>
      <div className="peers-container">
        {visible.length === 0 ? (
          <div className="roster-empty">
            <p className="empty-hint" style={{ color: theme.muted }}>
              {empty}
            </p>
            {showSettingsAction && (
              <button type="button" className="settings-inline-btn" onClick={onOpenSettings}>
                Open Settings
              </button>
            )}
          </div>
        ) : (
          visible.map((peer) => (
            <PeerRow
              key={peer.display_name}
              peer={peer}
              selected={selectedPeer === peer.display_name}
              busy={busy && selectedPeer === peer.display_name}
              statusMessage={statusMessage}
              now={now}
              onSelect={() => onSelectPeer(peer.display_name)}
              onFiles={(files) => onDropFiles(peer.display_name, files)}
              onDropError={onDropError}
            />
          ))
        )}
      </div>
    </section>
  );
}

function peerTimerLabel(lastSeenMs: number, now: number): string {
  if (lastSeenMs <= 0) return '';
  const age = now - lastSeenMs;
  const remaining = DEFAULT_STALE_MS - age;
  if (remaining <= 0) return 'expiring…';
  return `expires in ${formatSecondsLeft(remaining)}`;
}

function PeerRow({
  peer,
  selected,
  busy,
  statusMessage,
  now,
  onSelect,
  onFiles,
  onDropError,
}: {
  peer: PeerEntry;
  selected: boolean;
  busy: boolean;
  statusMessage: string;
  now: number;
  onSelect: () => void;
  onFiles: (files: File[]) => void | Promise<void>;
  onDropError?: (message: string) => void;
}) {
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [dragActive, setDragActive] = useState(false);
  const dragDepthRef = useRef(0);

  const allowDrop = !busy;

  const onDragEnter = (e: React.DragEvent) => {
    if (!allowDrop) {
      return;
    }
    e.preventDefault();
    dragDepthRef.current += 1;
    setDragActive(true);
  };

  const onDragLeave = (e: React.DragEvent) => {
    e.preventDefault();
    dragDepthRef.current -= 1;
    if (dragDepthRef.current <= 0) {
      dragDepthRef.current = 0;
      setDragActive(false);
    }
  };

  const onDragOver = (e: React.DragEvent) => {
    if (!allowDrop) {
      return;
    }
    e.preventDefault();
    e.dataTransfer.dropEffect = 'copy';
  };

  const handleDrop = async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    dragDepthRef.current = 0;
    setDragActive(false);
    if (!allowDrop) {
      return;
    }
    const files = await collectDropFiles(e.dataTransfer);
    if (files.length === 0) {
      onDropError?.('Could not read that drop — try Choose file instead');
      return;
    }
    await onFiles(files);
  };

  const onChooseFile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const list = e.target.files;
    if (!list?.length) {
      return;
    }
    void onFiles(Array.from(list));
    e.target.value = '';
  };

  return (
    <div
      className={`peer-row${selected ? ' selected' : ''}${dragActive ? ' drag-active' : ''}${busy ? ' busy' : ''}`}
      style={{ background: selected ? theme.rowSelected : theme.row }}
      onClick={onSelect}
      role="button"
      tabIndex={0}
      onKeyDown={(e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          onSelect();
        }
      }}
      onDragEnter={onDragEnter}
      onDragLeave={onDragLeave}
      onDragOver={onDragOver}
      onDrop={(e) => {
        void handleDrop(e);
      }}
    >
      <span className="presence-dot" style={{ background: theme.ok }} aria-hidden />
      <div className="peer-meta">
        <div className="peer-name">{peer.display_name}</div>
        <div className="peer-sub" style={{ color: theme.muted }}>
          {receiveStatusLabel(peer.receive_status)} · Port {peer.port_index}
          {peer.lastSeenMs > 0 && <> · {peerTimerLabel(peer.lastSeenMs, now)}</>}
        </div>
      </div>
      <div
        className={`drop-zone${busy ? ' disabled' : ''}${dragActive ? ' drag-active' : ''}`}
        style={{ background: theme.dropZone, borderColor: dragActive ? theme.accent : theme.accent + '66' }}
      >
        {busy ? (
          <span style={{ color: theme.muted }}>
            {isOutboundHandshakeWait(statusMessage)
              ? statusMessage.replace(/\s*\(\d+s\)\s*$/, '…')
              : 'Transfer in progress…'}
          </span>
        ) : (
          <>
            <span style={{ color: theme.accent }}>
              {dragActive ? 'Release to send' : `Drop a file to send to ${peer.display_name}`}
            </span>
            <button
              type="button"
              className="choose-file-btn"
              onClick={(e) => {
                e.stopPropagation();
                fileInputRef.current?.click();
              }}
            >
              Choose file…
            </button>
            <input
              ref={fileInputRef}
              type="file"
              hidden
              multiple
              onChange={onChooseFile}
              onClick={(e) => e.stopPropagation()}
            />
          </>
        )}
      </div>
    </div>
  );
}
