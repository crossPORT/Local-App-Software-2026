/** URL-only demo flags — no USB/WS I/O. */

export function fabricSimEnabled(): boolean {
  if (typeof window === 'undefined') {
    return false;
  }
  const value = new URLSearchParams(window.location.search).get('simulate');
  return value === '1' || value === 'true';
}

export function fabricPortFromUrl(): number {
  if (typeof window === 'undefined') {
    return 1;
  }
  const p = Number.parseInt(new URLSearchParams(window.location.search).get('port') ?? '1', 10);
  return Number.isFinite(p) && p >= 1 && p <= 4 ? p : 1;
}
