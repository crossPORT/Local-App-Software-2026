import type { IdentityProfile, PeerConfig, ReceiveStatus } from './types';

/** Internal preset used when display rate is enabled — not editable in UI. */
export const DISPLAY_RATE_PRESET = {
  display_rate_mib_s: 7168,
  display_rate_jitter_pct: 3,
} as const;

export function isDisplayRateDisabledInUrl(): boolean {
  return new URLSearchParams(window.location.search).get('display_rate') === '0';
}

function trim(value: string): string {
  return value.trim();
}

function expandHome(path: string): string {
  if (path.startsWith('~/')) {
    return path;
  }
  return path;
}

function parseReceiveStatus(value: string): ReceiveStatus {
  if (value === 'open' || value === 'auto' || value === 'auto_accept') {
    return 'open';
  }
  if (value === 'busy') {
    return 'busy';
  }
  return 'ask_first';
}

function applyIdentityKey(cfg: Partial<IdentityProfile>, key: string, value: string) {
  switch (key) {
    case 'display_name':
      cfg.display_name = value;
      break;
    case 'team':
      cfg.team = value;
      break;
    case 'role':
      cfg.role = value;
      break;
    case 'receive_status':
      cfg.receive_status = parseReceiveStatus(value);
      break;
    case 'receive_folder':
    case 'target_dir':
      cfg.receive_folder = value;
      break;
    case 'transfer_timeout_ms':
      cfg.transfer_timeout_ms = Number.parseInt(value, 10) || 0;
      break;
    case 'usb_inflight_mb':
      cfg.usb_inflight_mb = Number.parseInt(value, 10) || 0;
      break;
    case 'accept_ready_gap_ms':
      cfg.accept_ready_gap_ms = Math.max(0, Number.parseInt(value, 10) || 0);
      break;
    case 'accept_reply_delay_ms':
      cfg.accept_reply_delay_ms = Math.max(0, Number.parseInt(value, 10) || 0);
      break;
    case 'accept_timeout_sec':
      cfg.accept_timeout_sec = Math.max(0, Number.parseInt(value, 10) || 0);
      break;
    case 'ready_timeout_sec':
      cfg.ready_timeout_sec = Math.max(0, Number.parseInt(value, 10) || 0);
      break;
    case 'session_header_timeout_ms':
      cfg.session_header_timeout_ms = Math.max(0, Number.parseInt(value, 10) || 0);
      break;
    case 'payload_header_timeout_ms':
      cfg.payload_header_timeout_ms = Math.max(0, Number.parseInt(value, 10) || 0);
      break;
    case 'display_rate_mib_s':
      cfg.display_rate_mib_s = Math.max(0, Number.parseFloat(value) || 0);
      break;
    case 'display_rate_jitter_pct':
      cfg.display_rate_jitter_pct = Math.max(0, Number.parseFloat(value) || 0);
      break;
    case 'ep4_dynamic_switch':
    case 'experimental':
      cfg.ep4_dynamic_switch = value === '1' || value === 'true' || value === 'yes';
      break;
    default:
      break;
  }
}

function applyPeerKey(peer: PeerConfig, key: string, value: string) {
  switch (key) {
    case 'display_name':
      peer.display_name = value;
      break;
    case 'team':
      peer.team = value;
      break;
    case 'role':
      peer.role = value;
      break;
    case 'receive_status':
      peer.receive_status = parseReceiveStatus(value);
      break;
    case 'port_index':
      peer.port_index = Number.parseInt(value, 10) || 0;
      break;
    default:
      break;
  }
}

export function parseIdentityConfig(text: string, portIndex: number, configPath: string): IdentityProfile {
  const global: Partial<IdentityProfile> = {
    receive_status: 'ask_first',
    receive_folder: '~/Incoming',
    transfer_timeout_ms: 0,
    usb_inflight_mb: 0,
    accept_ready_gap_ms: 0,
    accept_reply_delay_ms: 0,
    accept_timeout_sec: 0,
    ready_timeout_sec: 0,
    session_header_timeout_ms: 0,
    payload_header_timeout_ms: 0,
    display_rate_mib_s: 0,
    display_rate_jitter_pct: 0,
    display_rate_enabled: true,
    ep4_dynamic_switch: false,
    peers: [],
  };
  const portCfg: Partial<IdentityProfile> = { peers: [] };
  let currentPeer: PeerConfig | null = null;
  let sectionPort = -1;

  for (const rawLine of text.split('\n')) {
    const line = trim(rawLine);
    if (!line || line.startsWith('#')) {
      continue;
    }

    const peerMatch = line.match(/^\[peer(\d+)\]$/);
    if (peerMatch) {
      if (currentPeer?.display_name) {
        global.peers!.push(currentPeer);
      }
      currentPeer = {
        display_name: '',
        team: '',
        role: '',
        receive_status: 'ask_first',
        port_index: 0,
      };
      sectionPort = -1;
      continue;
    }

    const portMatch = line.match(/^\[port(\d+)\]$/);
    if (portMatch) {
      if (currentPeer?.display_name) {
        global.peers!.push(currentPeer);
      }
      currentPeer = null;
      sectionPort = Number.parseInt(portMatch[1], 10);
      continue;
    }

    const eq = line.indexOf('=');
    if (eq < 0) {
      continue;
    }
    const key = trim(line.slice(0, eq));
    const value = expandHome(trim(line.slice(eq + 1)));

    if (currentPeer) {
      applyPeerKey(currentPeer, key, value);
    } else if (sectionPort < 0) {
      applyIdentityKey(global, key, value);
    } else if (sectionPort === portIndex) {
      applyIdentityKey(portCfg, key, value);
    }
  }

  if (currentPeer?.display_name) {
    global.peers!.push(currentPeer);
  }

  const profile: IdentityProfile = {
    display_name: portCfg.display_name || global.display_name || `System-${portIndex}`,
    team: portCfg.team || global.team || 'Team',
    role: portCfg.role || global.role || '',
    receive_status: portCfg.receive_status ?? global.receive_status ?? 'ask_first',
    receive_folder: portCfg.receive_folder || global.receive_folder || '~/Incoming',
    transfer_timeout_ms: portCfg.transfer_timeout_ms || global.transfer_timeout_ms || 0,
    usb_inflight_mb: portCfg.usb_inflight_mb || global.usb_inflight_mb || 0,
    accept_ready_gap_ms: portCfg.accept_ready_gap_ms || global.accept_ready_gap_ms || 0,
    accept_reply_delay_ms: portCfg.accept_reply_delay_ms || global.accept_reply_delay_ms || 0,
    accept_timeout_sec: portCfg.accept_timeout_sec || global.accept_timeout_sec || 0,
    ready_timeout_sec: portCfg.ready_timeout_sec || global.ready_timeout_sec || 0,
    session_header_timeout_ms:
      portCfg.session_header_timeout_ms || global.session_header_timeout_ms || 0,
    payload_header_timeout_ms:
      portCfg.payload_header_timeout_ms || global.payload_header_timeout_ms || 0,
    display_rate_mib_s:
      (portCfg.display_rate_mib_s ?? 0) > 0
        ? (portCfg.display_rate_mib_s ?? 0)
        : (global.display_rate_mib_s ?? 0),
    display_rate_jitter_pct:
      (portCfg.display_rate_jitter_pct ?? 0) > 0
        ? (portCfg.display_rate_jitter_pct ?? 0)
        : (global.display_rate_jitter_pct ?? 0),
    display_rate_enabled:
      (portCfg.display_rate_mib_s ?? 0) > 0 || (global.display_rate_mib_s ?? 0) > 0,
    ep4_dynamic_switch: portCfg.ep4_dynamic_switch ?? global.ep4_dynamic_switch ?? false,
    peers: global.peers ?? [],
    config_path: configPath,
    usb_read_buffer_size: portCfg.usb_read_buffer_size || global.usb_read_buffer_size || '256kb',
    announce_interval_sec: portCfg.announce_interval_sec || global.announce_interval_sec || 30,
  };

  return profile;
}

function identityStorageKey(portIndex: number): string {
  return `rocketbox-identity-v1-port${portIndex}`;
}

const GLOBAL_IDENTITY_KEY = 'rocketbox-identity-v1';

function readStoredIdentityJson(portIndex: number): string | null {
  if (typeof localStorage === 'undefined') {
    return null;
  }
  return (
    localStorage.getItem(identityStorageKey(portIndex)) ?? localStorage.getItem(GLOBAL_IDENTITY_KEY)
  );
}

export function defaultIdentityProfile(portIndex: number): IdentityProfile {
  return {
    display_name: '',
    team: '',
    role: '',
    receive_status: 'ask_first',
    receive_folder: '~/Incoming',
    transfer_timeout_ms: 0,
    usb_inflight_mb: 0,
    accept_ready_gap_ms: 0,
    accept_reply_delay_ms: 0,
    accept_timeout_sec: 0,
    ready_timeout_sec: 0,
    session_header_timeout_ms: 0,
    payload_header_timeout_ms: 0,
    display_rate_mib_s: 0,
    display_rate_jitter_pct: 0,
    display_rate_enabled: true,
    ep4_dynamic_switch: false,
    peers: [],
    config_path: `local:port${portIndex}`,
    usb_read_buffer_size: '256kb',
    announce_interval_sec: 30,
  };
}

function normalizePeer(raw: Partial<PeerConfig>): PeerConfig | null {
  const display_name = trim(raw.display_name ?? '');
  if (!display_name) {
    return null;
  }
  const port_index = Number(raw.port_index);
  return {
    display_name,
    team: trim(raw.team ?? ''),
    role: trim(raw.role ?? ''),
    receive_status: raw.receive_status ?? 'ask_first',
    port_index: Number.isFinite(port_index) && port_index >= 0 ? port_index : 0,
  };
}

function normalizeIdentity(raw: Partial<IdentityProfile>, portIndex: number): IdentityProfile {
  const defaults = defaultIdentityProfile(portIndex);
  return {
    ...defaults,
    ...raw,
    display_name: trim(raw.display_name ?? defaults.display_name),
    team: trim(raw.team ?? defaults.team),
    role: trim(raw.role ?? defaults.role),
    receive_status: raw.receive_status ?? defaults.receive_status,
    receive_folder: trim(raw.receive_folder ?? defaults.receive_folder),
    display_rate_enabled: raw.display_rate_enabled ?? defaults.display_rate_enabled,
    ep4_dynamic_switch: raw.ep4_dynamic_switch ?? defaults.ep4_dynamic_switch,
    peers: (raw.peers ?? [])
      .map((peer) => normalizePeer(peer))
      .filter((peer): peer is PeerConfig => peer != null),
    config_path: `local:port${portIndex}`,
    usb_read_buffer_size: raw.usb_read_buffer_size ?? defaults.usb_read_buffer_size,
    announce_interval_sec: raw.announce_interval_sec ?? defaults.announce_interval_sec,
  };
}

export function loadIdentityProfile(portIndex: number): IdentityProfile {
  try {
    const raw = readStoredIdentityJson(portIndex);
    if (!raw) {
      return defaultIdentityProfile(portIndex);
    }
    return normalizeIdentity(JSON.parse(raw) as Partial<IdentityProfile>, portIndex);
  } catch {
    return defaultIdentityProfile(portIndex);
  }
}

export function isDisplayRateEnabled(profile: IdentityProfile): boolean {
  return profile.display_rate_enabled;
}

export function displayRatePresetLabel(): string {
  const gib = DISPLAY_RATE_PRESET.display_rate_mib_s / 1024;
  const pct = DISPLAY_RATE_PRESET.display_rate_jitter_pct;
  return `~${Math.round(gib)} GiB/s (±${pct}%)`;
}

/** Apply internal preset rates when display rate is on; otherwise use measured speeds only. */
export function applyBoothDisplaySettings(
  profile: IdentityProfile,
  portIndex: number,
): IdentityProfile {
  if (isDisplayRateDisabledInUrl() || !profile.display_rate_enabled) {
    return normalizeIdentity(
      { ...profile, display_rate_mib_s: 0, display_rate_jitter_pct: 0 },
      portIndex,
    );
  }
  return normalizeIdentity(
    {
      ...profile,
      display_rate_mib_s: DISPLAY_RATE_PRESET.display_rate_mib_s,
      display_rate_jitter_pct: DISPLAY_RATE_PRESET.display_rate_jitter_pct,
    },
    portIndex,
  );
}

/** Load identity from localStorage and apply booth display preset when enabled. */
export async function loadIdentityProfileAsync(portIndex: number): Promise<IdentityProfile> {
  const stored = loadIdentityProfile(portIndex);
  let merged = normalizeIdentity(
    { ...stored, display_rate_mib_s: 0, display_rate_jitter_pct: 0 },
    portIndex,
  );
  if (isDisplayRateDisabledInUrl()) {
    merged = { ...merged, display_rate_enabled: false };
  }
  return applyBoothDisplaySettings(merged, portIndex);
}

export function saveIdentityProfile(portIndex: number, identity: IdentityProfile): void {
  const normalized = normalizeIdentity(
    {
      ...identity,
      display_rate_mib_s: 0,
      display_rate_jitter_pct: 0,
    },
    portIndex,
  );
  const json = JSON.stringify(normalized);
  localStorage.setItem(identityStorageKey(portIndex), json);
  localStorage.setItem(GLOBAL_IDENTITY_KEY, json);
}

export function receiveStatusToString(status: ReceiveStatus): string {
  switch (status) {
    case 'open':
      return 'open';
    case 'busy':
      return 'busy';
    default:
      return 'ask_first';
  }
}
