export type ReceiveStatus = 'open' | 'ask_first' | 'busy';

export interface PeerConfig {
  display_name: string;
  team: string;
  role: string;
  receive_status: ReceiveStatus;
  port_index: number;
}

export interface PeerEntry extends PeerConfig {
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
  booth_display_mib_s: number;
  booth_display_jitter_pct: number;
  booth_display_enabled: boolean;
  peers: PeerConfig[];
  config_path: string;
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

export interface AppUiState {
  identity: IdentityProfile;
  roster: PeerEntry[];
  portIndex: number;
  usbConnected: boolean;
  fabricConnected: boolean;
  fabricDevicesSeen: number;
  busy: boolean;
  waitingForPartner: boolean;
  statusMessage: string;
  errorMessage: string;
  notification: string;
  pendingOffer: PendingOffer | null;
  bytesDone: number;
  bytesTotal: number;
  liveMbps: number;
  fabricActivityMbps: number;
  fabricActivitySeq: number;
  peakMbps: number;
  resultMbps: number;
  boothDisplayMibS: number;
  transferLabel: string;
  selectedPeer: string;
  lastAnnounceMs: number;
}

export const initialUiState = (identity: IdentityProfile, portIndex: number): AppUiState => ({
  identity,
  roster: [],
  portIndex,
  usbConnected: false,
  fabricConnected: false,
  fabricDevicesSeen: 0,
  busy: false,
  waitingForPartner: false,
  statusMessage: '',
  errorMessage: '',
  notification: '',
  pendingOffer: null,
  bytesDone: 0,
  bytesTotal: 0,
  liveMbps: 0,
  fabricActivityMbps: 0,
  fabricActivitySeq: 0,
  peakMbps: 0,
  resultMbps: 0,
  boothDisplayMibS: 0,
  transferLabel: '',
  selectedPeer: '',
  lastAnnounceMs: 0,
});
