const DEV_ORIGIN_KEY = 'rocketbox_dev_origin';
const LEGACY_DEV_ORIGIN_KEY = 'rocketbox_booth_origin';

export function isLocalHostname(hostname: string): boolean {
  return hostname === 'localhost' || hostname === '127.0.0.1' || hostname === '[::1]';
}

export function buildPwaAppUrl(origin: string): string {
  const trimmed = origin.trim().replace(/\/+$/, '');
  return `${trimmed}/app`;
}

export function originFromWindow(): string {
  return window.location.origin;
}

export function defaultPwaAppUrl(): string {
  return buildPwaAppUrl(originFromWindow());
}

export function readSavedDevOrigin(): string | null {
  try {
    const saved =
      localStorage.getItem(DEV_ORIGIN_KEY)?.trim() ||
      localStorage.getItem(LEGACY_DEV_ORIGIN_KEY)?.trim();
    return saved ? saved.replace(/\/+$/, '') : null;
  } catch {
    return null;
  }
}

export function saveDevOrigin(origin: string): void {
  const normalized = origin.trim().replace(/\/+$/, '');
  localStorage.setItem(DEV_ORIGIN_KEY, normalized);
  try {
    localStorage.removeItem(LEGACY_DEV_ORIGIN_KEY);
  } catch {
    /* ignore */
  }
}

/** URL encoded in the QR — prefers a saved LAN origin when on localhost. */
export function resolvePwaAppUrl(savedOrigin: string | null = readSavedDevOrigin()): string {
  const { hostname } = window.location;
  if (!isLocalHostname(hostname)) {
    return defaultPwaAppUrl();
  }
  if (savedOrigin) {
    return buildPwaAppUrl(savedOrigin);
  }
  return defaultPwaAppUrl();
}

export function devOriginFromIp(ip: string, port: number | string, protocol = 'https:'): string {
  return `${protocol}//${ip}:${port}`;
}
