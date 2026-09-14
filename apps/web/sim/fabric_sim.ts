/** Simulated RocketBox is on by default. Use ?simulate=0 for real WebUSB. */

import { SIM_CABLE_SERIALS } from './fabric_hub';

const STORAGE_KEY = 'rocketbox-simulate';

export function fabricSimEnabled(): boolean {
  if (typeof window === 'undefined') {
    return true;
  }
  const value = new URLSearchParams(window.location.search).get('simulate');
  if (value === '0' || value === 'false') {
    return false;
  }
  if (value === '1' || value === 'true') {
    return true;
  }
  const stored = window.localStorage.getItem(STORAGE_KEY);
  if (stored === '0') {
    return false;
  }
  return true;
}

export function setFabricSimEnabled(enabled: boolean): void {
  if (typeof window === 'undefined') {
    return;
  }
  window.localStorage.setItem(STORAGE_KEY, enabled ? '1' : '0');
}

export function fabricSimDeviceCount(): number {
  return SIM_CABLE_SERIALS.length;
}
