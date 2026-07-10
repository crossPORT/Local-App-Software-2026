/** Enable only with ?simulate=1 (or true) on the PWA URL. */

export function fabricSimEnabled(): boolean {
  if (typeof window === 'undefined') {
    return false;
  }
  const value = new URLSearchParams(window.location.search).get('simulate');
  return value === '1' || value === 'true';
}

/** Fabric ports available in the simulation daemon. */
export function fabricSimDeviceCount(): number {
  return 4;
}
