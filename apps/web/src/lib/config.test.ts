import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  DISPLAY_RATE_PRESET,
  applyBoothDisplaySettings,
  displayRatePresetLabel,
  defaultIdentityProfile,
  isDisplayRateDisabledInUrl,
  isDisplayRateEnabled,
  loadIdentityProfile,
  parseIdentityConfig,
  receiveStatusToString,
  saveIdentityProfile,
} from './config';

function stubWindowSearch(search: string) {
  vi.stubGlobal('window', { location: { search } });
}

function stubLocalStorage() {
  const storage = new Map<string, string>();
  vi.stubGlobal('localStorage', {
    getItem: (key: string) => storage.get(key) ?? null,
    setItem: (key: string, value: string) => {
      storage.set(key, value);
    },
    removeItem: (key: string) => {
      storage.delete(key);
    },
    clear: () => {
      storage.clear();
    },
    key: () => null,
    get length() {
      return storage.size;
    },
  });
}

describe('parseIdentityConfig', () => {
  it('parses basic identity fields', () => {
    const profile = parseIdentityConfig(
      [
        'display_name=CAD-Workstation',
        'team=CAD',
        'receive_status=ask_first',
        'receive_folder=/tmp/inbox-cad',
      ].join('\n'),
      0,
      'test.conf',
    );
    expect(profile.display_name).toBe('CAD-Workstation');
    expect(profile.team).toBe('CAD');
    expect(profile.receive_status).toBe('ask_first');
    expect(profile.receive_folder).toBe('/tmp/inbox-cad');
  });

  it('parses tuning and booth display keys', () => {
    const profile = parseIdentityConfig(
      [
        'display_name=Node',
        'transfer_timeout_ms=5000',
        'usb_inflight_mb=64',
        'display_rate_mib_s=7168',
        'display_rate_jitter_pct=3',
      ].join('\n'),
      0,
      'test.conf',
    );
    expect(profile.transfer_timeout_ms).toBe(5000);
    expect(profile.usb_inflight_mb).toBe(64);
    expect(profile.display_rate_mib_s).toBe(7168);
    expect(profile.display_rate_jitter_pct).toBe(3);
    expect(profile.display_rate_enabled).toBe(true);
  });

  it('applies [portN] overrides for the active port only', () => {
    const profile = parseIdentityConfig(
      [
        'display_name=Global',
        'transfer_timeout_ms=1000',
        '[port1]',
        'display_name=Port-One',
        'transfer_timeout_ms=2000',
      ].join('\n'),
      1,
      'test.conf',
    );
    expect(profile.display_name).toBe('Port-One');
    expect(profile.transfer_timeout_ms).toBe(2000);

    const port0 = parseIdentityConfig(
      'display_name=Global\ntransfer_timeout_ms=1000\n[port1]\ntransfer_timeout_ms=2000\n',
      0,
      'test.conf',
    );
    expect(port0.display_name).toBe('Global');
    expect(port0.transfer_timeout_ms).toBe(1000);
  });

  it('parses peer sections', () => {
    const profile = parseIdentityConfig(
      [
        'display_name=Alice',
        '[peer0]',
        'display_name=Bob',
        'team=Creative',
        'receive_status=open',
        'port_index=1',
      ].join('\n'),
      0,
      'test.conf',
    );
    expect(profile.peers).toHaveLength(1);
    expect(profile.peers[0]).toMatchObject({
      display_name: 'Bob',
      team: 'Creative',
      receive_status: 'open',
      port_index: 1,
    });
  });

  it('normalizes receive_status aliases to open', () => {
    for (const status of ['open', 'auto', 'auto_accept']) {
      const profile = parseIdentityConfig(`receive_status=${status}\n`, 0, 'test.conf');
      expect(profile.receive_status).toBe('open');
    }
  });

  it('ignores comments and blank lines', () => {
    const profile = parseIdentityConfig(
      '# comment\n\nrole=sender\n   # trailing\n',
      0,
      'test.conf',
    );
    expect(profile.role).toBe('sender');
  });
});

describe('applyBoothDisplaySettings', () => {
  beforeEach(() => {
    stubWindowSearch('');
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('applies preset when booth display is enabled', () => {
    const profile = defaultIdentityProfile(0);
    const applied = applyBoothDisplaySettings({ ...profile, display_rate_enabled: true }, 0);
    expect(applied.display_rate_mib_s).toBe(DISPLAY_RATE_PRESET.display_rate_mib_s);
    expect(applied.display_rate_jitter_pct).toBe(DISPLAY_RATE_PRESET.display_rate_jitter_pct);
  });

  it('clears display rates when booth display is disabled', () => {
    const profile = defaultIdentityProfile(0);
    const applied = applyBoothDisplaySettings(
      {
        ...profile,
        display_rate_enabled: false,
        display_rate_mib_s: 7168,
        display_rate_jitter_pct: 3,
      },
      0,
    );
    expect(applied.display_rate_mib_s).toBe(0);
    expect(applied.display_rate_jitter_pct).toBe(0);
  });

  it('clears display rates when URL disables booth display', () => {
    stubWindowSearch('?display_rate=0');
    const profile = defaultIdentityProfile(0);
    const applied = applyBoothDisplaySettings({ ...profile, display_rate_enabled: true }, 0);
    expect(applied.display_rate_mib_s).toBe(0);
    expect(applied.display_rate_jitter_pct).toBe(0);
  });
});

describe('localStorage identity round-trip', () => {
  beforeEach(() => {
    stubLocalStorage();
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('returns defaults when nothing is stored', () => {
    const profile = loadIdentityProfile(0);
    expect(profile.display_name).toBe('');
    expect(profile.config_path).toBe('local:port0');
  });

  it('persists user-editable fields without booth rate secrets', () => {
    saveIdentityProfile(0, {
      ...defaultIdentityProfile(0),
      display_name: 'Alice',
      team: 'CAD',
      display_rate_enabled: true,
      display_rate_mib_s: 9999,
      display_rate_jitter_pct: 5,
    });
    const loaded = loadIdentityProfile(0);
    expect(loaded.display_name).toBe('Alice');
    expect(loaded.team).toBe('CAD');
    expect(loaded.display_rate_enabled).toBe(true);
    expect(loaded.display_rate_mib_s).toBe(0);
    expect(loaded.display_rate_jitter_pct).toBe(0);
  });
});

describe('config helpers', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('formats booth preset label', () => {
    expect(displayRatePresetLabel()).toMatch(/GiB\/s/);
  });

  it('detects booth display URL disable flag', () => {
    stubWindowSearch('');
    expect(isDisplayRateDisabledInUrl()).toBe(false);
    stubWindowSearch('?display_rate=0');
    expect(isDisplayRateDisabledInUrl()).toBe(true);
  });

  it('maps receive status to config strings', () => {
    expect(receiveStatusToString('open')).toBe('open');
    expect(receiveStatusToString('busy')).toBe('busy');
    expect(receiveStatusToString('ask_first')).toBe('ask_first');
  });

  it('reports booth display enabled from profile flag', () => {
    expect(isDisplayRateEnabled({ ...defaultIdentityProfile(0), display_rate_enabled: true })).toBe(
      true,
    );
  });
});
