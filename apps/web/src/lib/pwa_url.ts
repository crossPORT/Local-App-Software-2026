const BOOTH_ORIGIN_KEY = 'rocketbox_booth_origin';

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

export function readSavedBoothOrigin(): string | null {
  try {
    const saved = localStorage.getItem(BOOTH_ORIGIN_KEY)?.trim();
    return saved ? saved.replace(/\/+$/, '') : null;
  } catch {
    return null;
  }
}

export function saveBoothOrigin(origin: string): void {
  localStorage.setItem(BOOTH_ORIGIN_KEY, origin.trim().replace(/\/+$/, ''));
}

/** URL encoded in the booth QR — prefers a saved LAN origin when on localhost. */
export function resolvePwaAppUrl(savedOrigin: string | null = readSavedBoothOrigin()): string {
  const { hostname } = window.location;
  if (!isLocalHostname(hostname)) {
    return defaultPwaAppUrl();
  }
  if (savedOrigin) {
    return buildPwaAppUrl(savedOrigin);
  }
  return defaultPwaAppUrl();
}

export function boothOriginFromIp(ip: string, port: number | string, protocol = 'https:'): string {
  return `${protocol}//${ip}:${port}`;
}
