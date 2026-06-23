import { afterEach, describe, expect, it, vi } from 'vitest';
import { webUsbBlockedReason } from './webusb_env';

function stubBrowser(isSecure: boolean, hasUsb: boolean) {
  vi.stubGlobal('window', { isSecureContext: isSecure });
  vi.stubGlobal('navigator', hasUsb ? { usb: {} } : {});
}

describe('webUsbBlockedReason', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('reports HTTPS requirement on non-secure contexts', () => {
    stubBrowser(false, true);
    expect(webUsbBlockedReason()).toMatch(/HTTPS/);
  });

  it('returns null when secure and usb API exists', () => {
    stubBrowser(true, true);
    expect(webUsbBlockedReason()).toBeNull();
  });
});
