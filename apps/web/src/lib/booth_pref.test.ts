import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { defaultIdentityProfile, loadIdentityProfileAsync, saveIdentityProfile } from './config';

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
    clear: () => storage.clear(),
    key: () => null,
    get length() {
      return storage.size;
    },
  });
}

describe('booth display preference', () => {
  beforeEach(() => {
    stubLocalStorage();
    vi.stubGlobal('window', { location: { search: '' } });
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('remembers booth display off after reload', async () => {
    saveIdentityProfile(0, {
      ...defaultIdentityProfile(0),
      display_name: 'Alice',
      booth_display_enabled: false,
    });
    const loaded = await loadIdentityProfileAsync(0);
    expect(loaded.booth_display_enabled).toBe(false);
    expect(loaded.booth_display_mib_s).toBe(0);
  });

  it('remembers booth display on after reload', async () => {
    saveIdentityProfile(0, {
      ...defaultIdentityProfile(0),
      display_name: 'Alice',
      booth_display_enabled: true,
    });
    const loaded = await loadIdentityProfileAsync(1);
    expect(loaded.booth_display_enabled).toBe(true);
    expect(loaded.booth_display_mib_s).toBeGreaterThan(0);
  });
});
