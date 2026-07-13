import { RocketBoxError } from './errors';

/** RocketBox has four host-facing USB ports (internal index 0–3). */
export const PORT_COUNT = 4;

/** User-facing port number (1–4) from internal port index (0–3). */
export function toDisplayPort(portIndex: number): number {
  return portIndex + 1;
}

/** Internal port index (0–3) from user-facing port number (1–4). */
export function toPortIndex(displayPort: number): number {
  return displayPort - 1;
}

/** Normalize announce wire `port=` (1–4 preferred, 0–3 legacy) to internal index. */
export function portIndexFromWire(wirePort: number): number | null {
  if (Number.isFinite(wirePort) && wirePort >= 1 && wirePort <= PORT_COUNT) {
    return wirePort - 1;
  }
  if (Number.isFinite(wirePort) && wirePort >= 0 && wirePort < PORT_COUNT) {
    return wirePort;
  }
  return null;
}

/** Wire-format port number for announce notes. */
export function wirePortFromIndex(portIndex: number): number {
  return toDisplayPort(portIndex);
}

/** Derive port index (0–3) from the cable USB serial — sole source of port identity. */
export function portIndexFromSerial(serial: string): number {
  const trimmed = serial.trim();
  const n = Number.parseInt(trimmed, 16);
  if (!Number.isFinite(n) || n <= 0) {
    throw new RocketBoxError('Cable has no valid serial — cannot determine port', 'pairing');
  }
  return (n - 1) % PORT_COUNT;
}

export function resolvePortIndexFromDevice(device: { serialNumber?: string }): number {
  if (!device.serialNumber?.trim()) {
    throw new RocketBoxError('Cable has no USB serial — cannot determine port', 'pairing');
  }
  return portIndexFromSerial(device.serialNumber);
}

export function sortDevicesBySerial<T extends { serialNumber?: string }>(devices: T[]): T[] {
  return [...devices].sort((a, b) => (a.serialNumber ?? '').localeCompare(b.serialNumber ?? ''));
}

/** The three port indexes that are not {@link myPortIndex}, ascending order. */
export function remotePortIndexes(myPortIndex: number): number[] {
  const indexes: number[] = [];
  for (let i = 0; i < PORT_COUNT; i += 1) {
    if (i !== myPortIndex) {
      indexes.push(i);
    }
  }
  return indexes;
}

/** Display ports 1–4 excluding {@link myDisplayPort}. */
export function remoteDisplayPorts(myDisplayPort: number): number[] {
  return remotePortIndexes(toPortIndex(myDisplayPort)).map(toDisplayPort);
}

/**
 * State-1 idle partner for this display port: 1↔2, 3↔4.
 * Used to restore default pairing after announce burst / transfer idle.
 */
export function defaultPairDisplayPort(displayPort: number): number {
  if (displayPort === 1) return 2;
  if (displayPort === 2) return 1;
  if (displayPort === 3) return 4;
  if (displayPort === 4) return 3;
  throw new RocketBoxError(`Invalid display port ${displayPort}`, 'protocol');
}

/** UI label for a RocketBox port (display 1–4). */
export function formatPortLabel(portIndex: number, _serial?: string): string {
  return `Port ${toDisplayPort(portIndex)}`;
}
