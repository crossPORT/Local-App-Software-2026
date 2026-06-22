/** Enable with ?simulate=1 on the PWA URL (mirrors native ROCKETBOX_SIM=1). */

export function fabricSimEnabled(): boolean {
  if (typeof window === 'undefined') {
    return false;
  }
  const value = new URLSearchParams(window.location.search).get('simulate');
  return value === '1' || value === 'true';
}

export function fabricSimDeviceCount(): number {
  return 2;
}
