import { useState } from 'react';
import { boothDisplayPresetLabel, receiveStatusToString } from '../lib/config';
import { getEventLogLevel, setEventLogLevel, type EventLogLevel } from '../lib/event_log';
import { theme } from '../lib/theme';
import type { IdentityProfile, ReceiveStatus } from '../lib/types';

interface SettingsDialogProps {
  identity: IdentityProfile;
  onClose: () => void;
  onSave: (identity: IdentityProfile) => void;
  onOpenEventLog?: () => void;
}

export function SettingsDialog({ identity, onClose, onSave, onOpenEventLog }: SettingsDialogProps) {
  const [boothDisplayEnabled, setBoothDisplayEnabled] = useState(identity.booth_display_enabled);
  const [debugLogLevel, setDebugLogLevel] = useState<EventLogLevel>(() => getEventLogLevel());
  const [usbReadBufferSize, setUsbReadBufferSize] = useState(identity.usb_read_buffer_size ?? '256kb');
  const [error, setError] = useState('');

  const onSubmit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const data = new FormData(e.currentTarget);
    const display_name = String(data.get('display_name') ?? '').trim();
    if (!display_name) {
      setError('Enter a name for this computer.');
      return;
    }

    onSave({
      ...identity,
      display_name,
      team: String(data.get('team') ?? '').trim(),
      receive_status: String(data.get('receive_status') ?? 'ask_first') as ReceiveStatus,
      booth_display_enabled: boothDisplayEnabled,
      peers: [],
      usb_read_buffer_size: usbReadBufferSize,
      announce_interval_sec: parseInt(String(data.get('announce_interval_sec') ?? '30'), 10) || 30,
    });
  };

  return (
    <div className="modal-backdrop" role="presentation" onClick={onClose}>
      <div
        className="modal settings-dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="settings-title"
        onClick={(e) => e.stopPropagation()}
      >
        <h2 id="settings-title">Settings</h2>
        <p className="settings-hint" style={{ color: theme.muted }}>
          Your name is announced to other peers on the RocketBox fabric. Peer bookmarks are optional.
        </p>

        <form onSubmit={onSubmit}>
          <label>
            This computer
            <input
              name="display_name"
              type="text"
              required
              defaultValue={identity.display_name}
              placeholder="e.g. CAD-Workstation"
              autoComplete="off"
            />
          </label>

          <label>
            Group
            <input
              name="team"
              type="text"
              defaultValue={identity.team}
              placeholder="e.g. CAD"
              autoComplete="off"
            />
          </label>

          <label>
            When someone sends you a file
            <select name="receive_status" defaultValue={receiveStatusToString(identity.receive_status)}>
              <option value="open">Accept transfers automatically</option>
              <option value="ask_first">Ask before accepting</option>
              <option value="busy">Do not accept files</option>
            </select>
          </label>

          <label>
            USB Read Buffer Size
            <select
              value={usbReadBufferSize}
              onChange={(e) => setUsbReadBufferSize(e.target.value)}
            >
              <option value="16kb">16 KB Chunks (Standard USB legacy)</option>
              <option value="64kb">64 KB Chunks (High performance)</option>
              <option value="256kb">256 KB Chunks (Symmetrical matrix)</option>
              <option value="1mb">1 MB Chunks (Ultra-fast local loop)</option>
            </select>
          </label>

          <label>
            Announcement Period
            <select name="announce_interval_sec" defaultValue={String(identity.announce_interval_sec ?? 30)}>
              <option value="3">3 Seconds (High responsiveness)</option>
              <option value="5">5 Seconds (Fast discovery)</option>
              <option value="10">10 Seconds (Active matrix)</option>
              <option value="30">30 Seconds (Standard — less contention)</option>
              <option value="60">1 Minute (Conservative)</option>
            </select>
          </label>

          <div className="settings-toggle-row">
            <label className="settings-toggle-label">
              <input
                type="checkbox"
                checked={boothDisplayEnabled}
                onChange={(e) => setBoothDisplayEnabled(e.target.checked)}
              />
              <span>Booth display speed</span>
            </label>
            <p className="settings-hint" style={{ color: theme.muted }}>
              {boothDisplayEnabled
                ? `Transfer speeds use ${boothDisplayPresetLabel()} during active transfers.`
                : 'Transfer speeds reflect measured USB throughput.'}
            </p>
          </div>

          <div className="settings-toggle-row">
            <label className="settings-toggle-label">
              <span>Diagnostic log</span>
              <select
                value={debugLogLevel}
                onChange={(e) => {
                  const next = e.target.value as EventLogLevel;
                  setDebugLogLevel(next);
                  setEventLogLevel(next);
                }}
              >
                <option value="off">Off</option>
                <option value="normal">Normal</option>
                <option value="verbose">Verbose</option>
              </select>
            </label>
          </div>

          {onOpenEventLog && (
            <div className="settings-advanced-row">
              <button type="button" onClick={onOpenEventLog}>
                Event log…
              </button>
            </div>
          )}

          {error && (
            <p className="settings-error" style={{ color: theme.error }}>
              {error}
            </p>
          )}

          <div className="modal-actions">
            <button type="button" onClick={onClose}>
              Cancel
            </button>
            <button type="submit" className="primary">
              Save
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
