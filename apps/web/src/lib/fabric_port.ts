import { FabricUsbError } from './fabric_errors';

/** RocketBox fabric has four host-facing USB legs (internal index 0–3). */
export const FABRIC_LEG_COUNT = 4;

/** User-facing port number (1–4) from internal fabric leg (0–3). */
export function displayPortFromLeg(leg: number): number {
  return leg + 1;
}

/** Internal fabric leg (0–3) from user-facing port number (1–4). */
export function legFromDisplayPort(displayPort: number): number {
  return displayPort - 1;
}

/** Normalize announce wire `port=` (1–4 preferred, 0–3 legacy) to internal leg. */
export function legFromWirePort(wirePort: number): number | null {
  if (Number.isFinite(wirePort) && wirePort >= 1 && wirePort <= FABRIC_LEG_COUNT) {
    return wirePort - 1;
  }
  if (Number.isFinite(wirePort) && wirePort >= 0 && wirePort < FABRIC_LEG_COUNT) {
    return wirePort;
  }
  return null;
}

/** Wire-format port number for announce notes. */
export function wirePortFromLeg(leg: number): number {
  return displayPortFromLeg(leg);
}

/** Derive fabric leg (0–3) from the cable USB serial — sole source of leg identity. */
export function fabricLegFromSerial(serial: string): number {
  const trimmed = serial.trim();
  const n = Number.parseInt(trimmed, 16);
  if (!Number.isFinite(n) || n <= 0) {
    throw new FabricUsbError('Cable has no valid serial — cannot determine fabric leg');
  }
  return (n - 1) % FABRIC_LEG_COUNT;
}

export function resolveFabricLegFromDevice(device: { serialNumber?: string }): number {
  if (!device.serialNumber?.trim()) {
    throw new FabricUsbError('Cable has no USB serial — cannot determine fabric leg');
  }
  return fabricLegFromSerial(device.serialNumber);
}

/** @deprecated use fabricLegFromSerial */
export function fabricPortFromSerial(serial: string | undefined): number | null {
  if (!serial?.trim()) {
    return null;
  }
  try {
    return fabricLegFromSerial(serial);
  } catch {
    return null;
  }
}

export function sortFabricDevicesBySerial<T extends { serialNumber?: string }>(devices: T[]): T[] {
  return [...devices].sort((a, b) => (a.serialNumber ?? '').localeCompare(b.serialNumber ?? ''));
}

/** The three fabric legs that are not {@link myLeg}, ascending order. */
export function remoteFabricLegs(myLeg: number): number[] {
  const legs: number[] = [];
  for (let leg = 0; leg < FABRIC_LEG_COUNT; leg += 1) {
    if (leg !== myLeg) {
      legs.push(leg);
    }
  }
  return legs;
}

/** Short booth / event-log label for a fabric port (display 1–4). */
export function formatFabricLegLabel(leg: number, _serial?: string): string {
  return `Port ${displayPortFromLeg(leg)}`;
}

/** Primary UI label for the connected fabric port. */
export function formatFabricPortDisplay(leg: number, _serial?: string): string {
  return formatFabricLegLabel(leg);
}
