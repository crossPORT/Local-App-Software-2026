/** Enable with ?simulate=1 on the PWA URL (mirrors native ROCKETBOX_SIM=1). */

import { SIM_CABLE_SERIALS } from './fabric_hub';

export function fabricSimEnabled(): boolean {
  if (typeof window === 'undefined' || typeof localStorage === 'undefined') {
    return false;
  }
  const params = new URLSearchParams(window.location.search);
  const value = params.get('simulate');
  if (value === '1' || value === 'true') {
    localStorage.setItem('rocketbox_simulate', 'true');
    return true;
  }
  if (value === '0' || value === 'false') {
    localStorage.setItem('rocketbox_simulate', 'false');
    return false;
  }
  
  const stored = localStorage.getItem('rocketbox_simulate');
  if (stored !== null) {
    return stored === 'true';
  }
  
  // Default to simulation on localhost or 127.0.0.1
  return window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
}

export function fabricSimDeviceCount(): number {
  return SIM_CABLE_SERIALS.length;
}
