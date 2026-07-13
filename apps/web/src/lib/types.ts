export type ReceiveStatus = 'open' | 'ask_first' | 'busy';

export interface PeerConfig {
  display_name: string;
  team: string;
  role: string;
  receive_status: ReceiveStatus;
  port_index: number;
}

export interface PeerEntry extends PeerConfig {
  id: string;
  instance_id: string;
  online: boolean;
  lastSeenMs: number;
}

export interface IdentityProfile {
  display_name: string;
  team: string;
  role: string;
  receive_status: ReceiveStatus;
  receive_folder: string;
  transfer_timeout_ms: number;
  usb_inflight_mb: number;
  accept_ready_gap_ms: number;
  accept_reply_delay_ms: number;
  accept_timeout_sec: number;
  ready_timeout_sec: number;
  session_header_timeout_ms: number;
  payload_header_timeout_ms: number;
  display_rate_mib_s: number;
  display_rate_jitter_pct: number;
  display_rate_enabled: boolean;
  peers: PeerConfig[];
  config_path: string;
  usb_read_buffer_size?: string;
  announce_interval_sec?: number;
}

export interface PendingOffer {
  from_name: string;
  team: string;
  payload_name: string;
  total_bytes: number;
  file_count: number;
  note: string;
  session_id: string;
}

export type TransferPhase = 'idle' | 'waiting' | 'transferring' | 'complete' | 'failed';

/** Peer-card link icon: absent / attempting / established. */
export type LinkUiState = 'none' | 'linking' | 'linked';

export interface AppUiState {
  identity: IdentityProfile;
  roster: PeerEntry[];
  portIndex: number;
  usbConnected: boolean;
  devicesSeen: number;
  /** True when this origin has a remembered cable serial in sessionStorage. */
  hasSavedCable: boolean;
  busy: boolean;
  waitingForPartner: boolean;
  statusMessage: string;
  errorMessage: string;
  notification: string;
  pendingOffer: PendingOffer | null;
  bytesDone: number;
  bytesTotal: number;
  liveMbps: number;
  usbActivityMbps: number;
  usbActivitySeq: number;
  peakMbps: number;
  resultMbps: number;
  displayRateMibS: number;
  transferLabel: string;
  selectedPeer: string;
  lastAnnounceMs: number;
  /** Display port 1–4 when linkState ≠ none; 0 = cleared. */
  linkedPort: number;
  /** none = no icon; linking = attempt; linked = both sides evidenced. */
  linkState: LinkUiState;
}

export const initialUiState = (identity: IdentityProfile, portIndex: number): AppUiState => ({
  identity,
  roster: [],
  portIndex,
  usbConnected: false,
  devicesSeen: 0,
  hasSavedCable: false,
  busy: false,
  waitingForPartner: false,
  statusMessage: '',
  errorMessage: '',
  notification: '',
  pendingOffer: null,
  bytesDone: 0,
  bytesTotal: 0,
  liveMbps: 0,
  usbActivityMbps: 0,
  usbActivitySeq: 0,
  peakMbps: 0,
  resultMbps: 0,
  displayRateMibS: 0,
  transferLabel: '',
  selectedPeer: '',
  lastAnnounceMs: 0,
  linkedPort: 0,
  linkState: 'none',
});
