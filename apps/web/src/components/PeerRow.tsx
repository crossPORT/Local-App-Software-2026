import { toDisplayPort } from '@rocketbox/sdk';
import { useRef, useState } from 'react';
import { collectDropFiles } from '../lib/collect_drop_files';
import { isOutboundHandshakeWait, receiveStatusLabel } from '../lib/format';
import { DEFAULT_STALE_MS } from '../lib/peer_roster';
import { theme } from '../lib/theme';
import type { LinkUiState, PeerEntry } from '../lib/types';
import { PeerLinkIcon } from './PeerLinkIcon';

function peerTimerLabel(lastSeenMs: number, now: number): string {
  if (lastSeenMs <= 0) return '';
  const remaining = DEFAULT_STALE_MS - (now - lastSeenMs);
  if (remaining <= 0) return 'expiring…';
  return `expires in ${Math.max(0, Math.ceil(remaining / 1000))}s`;
}

export function PeerRow({
  peer,
  label,
  offline,
  leg = -1,
  selected,
  busy,
  linkState = 'none',
  statusMessage,
  now,
  onSelect,
  onFiles,
  onDropError,
  onReleaseLink,
}: {
  peer: PeerEntry | null;
  label: string;
  offline: boolean;
  leg?: number;
  selected: boolean;
  busy: boolean;
  linkState?: LinkUiState;
  statusMessage: string;
  now: number;
  onSelect: () => void;
  onFiles: (files: File[]) => void | Promise<void>;
  onDropError?: (message: string) => void;
  onReleaseLink?: () => void;
}) {
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [dragActive, setDragActive] = useState(false);
  const dragDepthRef = useRef(0);
  const allowDrop = !busy && !offline;
  const showLink = !!peer && (linkState === 'linking' || linkState === 'linked');
  const linkTitle = !peer
    ? ''
    : linkState === 'linking'
      ? `Linking to port ${toDisplayPort(peer.port_index)}… — click to release`
      : `Linked to port ${toDisplayPort(peer.port_index)} — click to release`;

  const onDragEnter = (e: React.DragEvent) => {
    if (!allowDrop) return;
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
    if (!allowDrop) return;
    e.preventDefault();
    e.dataTransfer.dropEffect = 'copy';
  };
  const handleDrop = async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    dragDepthRef.current = 0;
    setDragActive(false);
    if (!allowDrop) return;
    const files = await collectDropFiles(e.dataTransfer);
    if (files.length === 0) {
      onDropError?.('Could not read that drop — try Choose file instead');
      return;
    }
    await onFiles(files);
  };

  return (
    <div
      className={`peer-row${selected ? ' selected' : ''}${dragActive ? ' drag-active' : ''}${busy ? ' busy' : ''}${offline ? ' offline' : ''}`}
      style={{ background: offline ? theme.offlineRow : selected ? theme.rowSelected : theme.row }}
      onClick={offline ? undefined : onSelect}
      role={offline ? undefined : 'button'}
      tabIndex={offline ? -1 : 0}
      onKeyDown={
        offline
          ? undefined
          : (e) => {
              if (e.key === 'Enter' || e.key === ' ') onSelect();
            }
      }
      onDragEnter={onDragEnter}
      onDragLeave={onDragLeave}
      onDragOver={onDragOver}
      onDrop={(e) => {
        void handleDrop(e);
      }}
    >
      {showLink && (linkState === 'linking' || linkState === 'linked') ? (
        <PeerLinkIcon state={linkState} title={linkTitle} onClick={onReleaseLink} />
      ) : null}
      <span
        className="presence-dot"
        style={{ background: offline ? theme.offlineDot : theme.ok }}
        aria-hidden
      />
      <div className="peer-meta">
        <div className="peer-name" style={{ color: offline ? theme.muted : theme.text }}>
          {label}
        </div>
        <div className="peer-sub" style={{ color: theme.muted }}>
          {offline ? (
            <>Not connected — waiting for peer · port {toDisplayPort(leg)}</>
          ) : (
            <>
              {receiveStatusLabel(peer!.receive_status)} · port {toDisplayPort(peer!.port_index)}
              {peer!.lastSeenMs > 0 && <> · {peerTimerLabel(peer!.lastSeenMs, now)}</>}
            </>
          )}
        </div>
      </div>
      <div
        className={`drop-zone${busy || offline ? ' disabled' : ''}${dragActive ? ' drag-active' : ''}`}
        style={{
          background: offline ? theme.offlineDropZone : theme.dropZone,
          borderColor: dragActive ? theme.accent : theme.accent + '66',
        }}
      >
        {offline ? (
          <span style={{ color: theme.muted }}>Not connected</span>
        ) : busy ? (
          <span style={{ color: theme.muted }}>
            {isOutboundHandshakeWait(statusMessage)
              ? statusMessage.replace(/\s*\(\d+s\)\s*$/, '…')
              : 'Transfer in progress…'}
          </span>
        ) : (
          <>
            <span style={{ color: theme.accent }}>
              {dragActive ? 'Release to send' : `Drop a file to send to ${label}`}
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
              accept="*/*"
              hidden
              multiple
              onChange={(e) => {
                const list = e.target.files;
                if (!list?.length) return;
                void onFiles(Array.from(list));
                e.target.value = '';
              }}
              onClick={(e) => e.stopPropagation()}
            />
          </>
        )}
      </div>
    </div>
  );
}
