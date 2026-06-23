/** Stable RocketBox fabric port (0 or 1) for a USB cable serial. */
export function fabricPortFromSerial(serial: string | undefined): number | null {
  const trimmed = serial?.trim();
  if (!trimmed) {
    return null;
  }
  const n = Number.parseInt(trimmed, 16);
  if (!Number.isFinite(n) || n <= 0) {
    return null;
  }
  return (n - 1) % 2;
}

export function readUrlFabricPort(): number | null {
  if (typeof window === 'undefined') {
    return null;
  }
  const port = Number.parseInt(new URLSearchParams(window.location.search).get('port') ?? '', 10);
  if (Number.isFinite(port) && port >= 0 && port <= 1) {
    return port;
  }
  return null;
}

function fabricPortStorageKey(serial: string): string {
  return `rocketbox_fabric_port_${serial}`;
}

export function savedFabricPortForSerial(serial: string | undefined): number | null {
  const trimmed = serial?.trim();
  if (!trimmed || typeof sessionStorage === 'undefined') {
    return null;
  }
  const saved = sessionStorage.getItem(fabricPortStorageKey(trimmed));
  if (saved === '0' || saved === '1') {
    return Number(saved);
  }
  return null;
}

export function rememberFabricPortForSerial(serial: string | undefined, portIndex: number): void {
  const trimmed = serial?.trim();
  if (!trimmed || portIndex < 0 || portIndex > 1 || typeof sessionStorage === 'undefined') {
    return;
  }
  sessionStorage.setItem(fabricPortStorageKey(trimmed), String(portIndex));
}

export function sortFabricDevicesBySerial<T extends { serialNumber?: string }>(devices: T[]): T[] {
  return [...devices].sort((a, b) => (a.serialNumber ?? '').localeCompare(b.serialNumber ?? ''));
}

/**
 * Resolve this station's fabric port for announces and roster logic.
 * Multi-cable hosts use device sort order; single-cable PWAs use the serial suffix.
 */
export function resolveFabricPortIndex(
  device: { serialNumber?: string },
  devices: { serialNumber?: string }[],
  portHint = 0,
): number {
  const urlPort = readUrlFabricPort();
  if (urlPort !== null) {
    return urlPort;
  }

  const serial = device.serialNumber?.trim();
  const fromSerial = serial ? fabricPortFromSerial(serial) : null;

  const sorted = sortFabricDevicesBySerial(devices);
  if (sorted.length > 1) {
    const index = sorted.findIndex((candidate) => candidate === device);
    if (index >= 0) {
      return index;
    }
  }

  if (fromSerial !== null) {
    const savedPort = savedFabricPortForSerial(serial);
    if (savedPort !== null && savedPort !== fromSerial && typeof sessionStorage !== 'undefined') {
      sessionStorage.removeItem(fabricPortStorageKey(serial));
    }
    return fromSerial;
  }

  const savedPort = savedFabricPortForSerial(serial);
  if (savedPort !== null) {
    return savedPort;
  }

  return portHint >= 0 && portHint <= 1 ? portHint : 0;
}
