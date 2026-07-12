/** Why WebUSB is blocked, or null if usable. */
export function webUsbBlockedReason(): string | null {
  if (typeof window === 'undefined') {
    return 'WebUSB is not available in this environment.';
  }
  if (!window.isSecureContext) {
    return 'WebUSB requires HTTPS — use https:// on this device and accept the certificate warning.';
  }
  if (!navigator.usb) {
    return 'WebUSB is not available — use Chrome or Edge.';
  }
  return null;
}

export function isWebUsbAvailable(): boolean {
  return webUsbBlockedReason() === null;
}
