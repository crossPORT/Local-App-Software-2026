import { FabricUsbError } from './errors';

/** RocketBox fabric has four host-facing USB legs (internal index 0–3). */
export const FABRIC_LEG_COUNT = 4;
export const PORT_COUNT = FABRIC_LEG_COUNT;

export function displayPortFromLeg(leg: number): number {
  return leg + 1;
}

export function toDisplayPort(leg: number): number {
  return displayPortFromLeg(leg);
}

export function legFromDisplayPort(displayPort: number): number {
  return displayPort - 1;
}

export function toPortIndex(displayPort: number): number {
  return legFromDisplayPort(displayPort);
}

export function legFromWirePort(wirePort: number): number | null {
  if (Number.isFinite(wirePort) && wirePort >= 1 && wirePort <= FABRIC_LEG_COUNT) {
    return wirePort - 1;
  }
  if (Number.isFinite(wirePort) && wirePort >= 0 && wirePort < FABRIC_LEG_COUNT) {
    return wirePort;
  }
  return null;
}

export function wirePortFromLeg(leg: number): number {
  return displayPortFromLeg(leg);
}

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

export function remoteFabricLegs(myLeg: number): number[] {
  const legs: number[] = [];
  for (let leg = 0; leg < FABRIC_LEG_COUNT; leg += 1) {
    if (leg !== myLeg) {
      legs.push(leg);
    }
  }
  return legs;
}

export function formatFabricLegLabel(leg: number, _serial?: string): string {
  return `Port ${displayPortFromLeg(leg)}`;
}

export function formatFabricPortDisplay(leg: number, _serial?: string): string {
  return formatFabricLegLabel(leg);
}

export function formatPortLabel(leg: number, serial?: string): string {
  return formatFabricLegLabel(leg, serial);
}
