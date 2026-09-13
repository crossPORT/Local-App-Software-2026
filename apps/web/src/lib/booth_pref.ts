const BOOTH_DISPLAY_PREF_KEY = 'rocketbox-booth-display-enabled';

export function readBoothDisplayPref(): boolean | null {
  if (typeof localStorage === 'undefined') {
    return null;
  }
  const raw = localStorage.getItem(BOOTH_DISPLAY_PREF_KEY);
  if (raw === '1' || raw === 'true') {
    return true;
  }
  if (raw === '0' || raw === 'false') {
    return false;
  }
  return null;
}

export function writeBoothDisplayPref(enabled: boolean): void {
  if (typeof localStorage === 'undefined') {
    return;
  }
  localStorage.setItem(BOOTH_DISPLAY_PREF_KEY, enabled ? '1' : '0');
}
