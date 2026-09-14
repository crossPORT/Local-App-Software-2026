import { SIM_CABLE_SERIALS } from './fabric_hub';

const SELECTED_SERIAL_KEY = 'rocketbox_sim_serial';

export function getSavedSimSerial(): string | null {
  return sessionStorage.getItem(SELECTED_SERIAL_KEY);
}

export function rememberSimSerial(serial: string): void {
  sessionStorage.setItem(SELECTED_SERIAL_KEY, serial);
}

export function clearSavedSimSerial(): void {
  sessionStorage.removeItem(SELECTED_SERIAL_KEY);
}

export function pickSimSerial(preferred?: string): string {
  if (preferred && SIM_CABLE_SERIALS.includes(preferred as (typeof SIM_CABLE_SERIALS)[number])) {
    return preferred;
  }
  const saved = getSavedSimSerial();
  if (saved && SIM_CABLE_SERIALS.includes(saved as (typeof SIM_CABLE_SERIALS)[number])) {
    return saved;
  }
  return SIM_CABLE_SERIALS[0]!;
}
