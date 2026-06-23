import { useState } from 'react';
import { boothDisplayPresetLabel, receiveStatusToString } from '../lib/config';
import { getBoothLogLevel, setBoothLogLevel, type BoothLogLevel } from '../lib/booth_log';
import { FABRIC_LEG_COUNT, displayPortFromLeg } from '../lib/fabric_port';
import { theme } from '../lib/theme';
import type { IdentityProfile, PeerConfig, ReceiveStatus } from '../lib/types';

interface SettingsDialogProps {
  identity: IdentityProfile;
  portIndex: number;
  onClose: () => void;
  onSave: (identity: IdentityProfile) => void;
  onOpenEventLog?: () => void;
}

function emptyPeer(portIndex: number): PeerConfig {
  return {
    display_name: '',
    team: '',
    role: '',
    receive_status: 'ask_first',
    port_index: portIndex === 0 ? 1 : 0,
  };
}

export function SettingsDialog({ identity, portIndex, onClose, onSave, onOpenEventLog }: SettingsDialogProps) {
  const [peers, setPeers] = useState<PeerConfig[]>(
    identity.peers.length > 0 ? identity.peers.map((peer) => ({ ...peer })) : [],
  );
  const [boothDisplayEnabled, setBoothDisplayEnabled] = useState(identity.booth_display_enabled);
  const [debugLogLevel, setDebugLogLevel] = useState<BoothLogLevel>(() => getBoothLogLevel());
  const [error, setError] = useState('');

  const onSubmit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const data = new FormData(e.currentTarget);
    const display_name = String(data.get('display_name') ?? '').trim();
    if (!display_name) {
      setError('Enter a name for this computer.');
      return;
    }

    const nextPeers = peers
      .map((peer, index) => ({
        ...peer,
        display_name: String(data.get(`peer_${index}_name`) ?? '').trim(),
        port_index: Number.parseInt(String(data.get(`peer_${index}_port`) ?? '0'), 10) || 0,
      }))
      .filter((peer) => peer.display_name);

    onSave({
      ...identity,
      display_name,
      team: String(data.get('team') ?? '').trim(),
      receive_status: String(data.get('receive_status') ?? 'ask_first') as ReceiveStatus,
      booth_display_enabled: boothDisplayEnabled,
      peers: nextPeers,
    });
  };

  const addPeer = () => {
    setPeers((current) => [...current, emptyPeer(portIndex)]);
  };

  const removePeer = (index: number) => {
    setPeers((current) => (current.length <= 1 ? current : current.filter((_, i) => i !== index)));
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
              <option value="open">Save automatically</option>
              <option value="ask_first">Ask before saving</option>
              <option value="busy">Do not accept files</option>
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
                  const next = e.target.value as BoothLogLevel;
                  setDebugLogLevel(next);
                  setBoothLogLevel(next);
                }}
              >
                <option value="off">Off</option>
                <option value="normal">Normal</option>
                <option value="verbose">Verbose</option>
              </select>
            </label>
            {onOpenEventLog && (
              <button type="button" className="settings-inline-btn" onClick={onOpenEventLog}>
                Open event log
              </button>
            )}
          </div>

          <div className="settings-section">
            <div className="settings-section-head">
              <h3>Peer bookmarks (optional)</h3>
              <button type="button" className="settings-inline-btn" onClick={addPeer}>
                Add bookmark
              </button>
            </div>
            <p className="settings-hint" style={{ color: theme.muted }}>
              Connected peers are discovered automatically. Bookmarks are only needed to pre-label a known station.
            </p>
            {peers.length === 0 ? (
              <p className="settings-hint" style={{ color: theme.muted }}>
                No bookmarks yet.
              </p>
            ) : (
              peers.map((peer, index) => (
              <div key={index} className="peer-settings-row">
                <label>
                  Name
                  <input
                    name={`peer_${index}_name`}
                    type="text"
                    defaultValue={peer.display_name}
                    placeholder="Peer name"
                    autoComplete="off"
                  />
                </label>
                <label>
                  Fabric port
                  <select name={`peer_${index}_port`} defaultValue={String(peer.port_index)}>
                    {Array.from({ length: FABRIC_LEG_COUNT }, (_, leg) => (
                      <option key={leg} value={String(leg)}>
                        Port {displayPortFromLeg(leg)}
                      </option>
                    ))}
                  </select>
                </label>
                {peers.length > 1 && (
                  <button type="button" className="settings-inline-btn" onClick={() => removePeer(index)}>
                    Remove
                  </button>
                )}
              </div>
              ))
            )}
          </div>

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
