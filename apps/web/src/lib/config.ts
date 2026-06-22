import type { IdentityProfile, PeerConfig, ReceiveStatus } from './types';

/** Internal preset used when booth display speed is enabled — not editable in UI. */
export const BOOTH_DISPLAY_PRESET = {
  booth_display_mib_s: 7168,
  booth_display_jitter_pct: 3,
} as const;

export function isBoothDisplayDisabledInUrl(): boolean {
  return new URLSearchParams(window.location.search).get('booth_display') === '0';
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
    case 'booth_display_mib_s':
      cfg.booth_display_mib_s = Math.max(0, Number.parseFloat(value) || 0);
      break;
    case 'booth_display_jitter_pct':
      cfg.booth_display_jitter_pct = Math.max(0, Number.parseFloat(value) || 0);
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
    booth_display_mib_s: 0,
    booth_display_jitter_pct: 0,
    booth_display_enabled: true,
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
    booth_display_mib_s:
      (portCfg.booth_display_mib_s ?? 0) > 0
        ? (portCfg.booth_display_mib_s ?? 0)
        : (global.booth_display_mib_s ?? 0),
    booth_display_jitter_pct:
      (portCfg.booth_display_jitter_pct ?? 0) > 0
        ? (portCfg.booth_display_jitter_pct ?? 0)
        : (global.booth_display_jitter_pct ?? 0),
    booth_display_enabled:
      (portCfg.booth_display_mib_s ?? 0) > 0 || (global.booth_display_mib_s ?? 0) > 0,
    peers: global.peers ?? [],
    config_path: configPath,
  };

  return profile;
}

function identityStorageKey(portIndex: number): string {
  return `rocketbox-identity-v1-port${portIndex}`;
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
    booth_display_mib_s: 0,
    booth_display_jitter_pct: 0,
    booth_display_enabled: true,
    peers: [],
    config_path: `local:port${portIndex}`,
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
    booth_display_enabled: raw.booth_display_enabled ?? defaults.booth_display_enabled,
    peers: (raw.peers ?? [])
      .map((peer) => normalizePeer(peer))
      .filter((peer): peer is PeerConfig => peer != null),
    config_path: `local:port${portIndex}`,
  };
}

export function loadIdentityProfile(portIndex: number): IdentityProfile {
  try {
    const raw = localStorage.getItem(identityStorageKey(portIndex));
    if (!raw) {
      return defaultIdentityProfile(portIndex);
    }
    return normalizeIdentity(JSON.parse(raw) as Partial<IdentityProfile>, portIndex);
  } catch {
    return defaultIdentityProfile(portIndex);
  }
}

function mergeBoothIdentity(
  booth: IdentityProfile,
  local: IdentityProfile,
  portIndex: number,
): IdentityProfile {
  return normalizeIdentity(
    {
      ...booth,
      display_name: local.display_name.trim() ? local.display_name : booth.display_name,
      team: local.team.trim() ? local.team : booth.team,
      role: local.role.trim() ? local.role : booth.role,
      receive_status: local.receive_status ?? booth.receive_status,
      receive_folder: local.receive_folder.trim() ? local.receive_folder : booth.receive_folder,
      peers: booth.peers.length > 0 ? booth.peers : local.peers,
      booth_display_enabled: local.booth_display_enabled,
    },
    portIndex,
  );
}

export function isBoothDisplayEnabled(profile: IdentityProfile): boolean {
  return profile.booth_display_enabled;
}

export function boothDisplayPresetLabel(): string {
  const gib = BOOTH_DISPLAY_PRESET.booth_display_mib_s / 1024;
  const pct = BOOTH_DISPLAY_PRESET.booth_display_jitter_pct;
  return `~${Math.round(gib)} GiB/s (±${pct}%)`;
}

/** Apply internal preset rates when booth display speed is on; otherwise use measured speeds only. */
export function applyBoothDisplaySettings(
  profile: IdentityProfile,
  portIndex: number,
): IdentityProfile {
  if (isBoothDisplayDisabledInUrl() || !profile.booth_display_enabled) {
    return normalizeIdentity(
      { ...profile, booth_display_mib_s: 0, booth_display_jitter_pct: 0 },
      portIndex,
    );
  }
  return normalizeIdentity(
    {
      ...profile,
      booth_display_mib_s: BOOTH_DISPLAY_PRESET.booth_display_mib_s,
      booth_display_jitter_pct: BOOTH_DISPLAY_PRESET.booth_display_jitter_pct,
    },
    portIndex,
  );
}

async function fetchBoothConfig(portIndex: number): Promise<IdentityProfile | null> {
  try {
    const base = import.meta.env.BASE_URL;
    const response = await fetch(`${base}booth-port${portIndex}.conf`, { cache: 'no-store' });
    if (!response.ok) {
      return null;
    }
    const text = await response.text();
    if (!text.includes('booth_display_mib_s')) {
      return null;
    }
    return parseIdentityConfig(text, portIndex, `booth-port${portIndex}.conf`);
  } catch {
    return null;
  }
}

/** Load local identity, then overlay booth defaults from /booth-portN.conf when served. */
export async function loadIdentityProfileAsync(portIndex: number): Promise<IdentityProfile> {
  const stored = loadIdentityProfile(portIndex);
  const local = normalizeIdentity(
    { ...stored, booth_display_mib_s: 0, booth_display_jitter_pct: 0 },
    portIndex,
  );
  const booth = await fetchBoothConfig(portIndex);
  let merged = booth ? mergeBoothIdentity(booth, local, portIndex) : local;
  if (booth && stored.config_path.startsWith('local:') && !stored.display_name.trim()) {
    merged = { ...merged, booth_display_enabled: booth.booth_display_mib_s > 0 };
  }
  if (isBoothDisplayDisabledInUrl()) {
    merged = { ...merged, booth_display_enabled: false };
  }
  return applyBoothDisplaySettings(merged, portIndex);
}

export function saveIdentityProfile(portIndex: number, identity: IdentityProfile): void {
  const normalized = normalizeIdentity(
    {
      ...identity,
      booth_display_mib_s: 0,
      booth_display_jitter_pct: 0,
    },
    portIndex,
  );
  localStorage.setItem(identityStorageKey(portIndex), JSON.stringify(normalized));
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
