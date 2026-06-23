import { describe, expect, it, vi } from 'vitest';
import {
  boothOriginFromIp,
  buildPwaAppUrl,
  isLocalHostname,
  resolvePwaAppUrl,
} from './pwa_url';

describe('pwa_url', () => {
  it('detects local hostnames', () => {
    expect(isLocalHostname('localhost')).toBe(true);
    expect(isLocalHostname('127.0.0.1')).toBe(true);
    expect(isLocalHostname('192.168.1.42')).toBe(false);
  });

  it('builds app URL from origin', () => {
    expect(buildPwaAppUrl('https://192.168.1.5:8080')).toBe('https://192.168.1.5:8080/app');
  });

  it('builds booth origin from LAN IP', () => {
    expect(boothOriginFromIp('10.0.0.8', 8080)).toBe('https://10.0.0.8:8080');
  });

  it('prefers saved origin when hostname is localhost', () => {
    vi.stubGlobal('window', {
      location: {
        hostname: 'localhost',
        origin: 'https://localhost:8080',
        port: '8080',
        protocol: 'https:',
      },
    });
    try {
      const saved = 'https://192.168.0.12:8080';
      expect(resolvePwaAppUrl(saved)).toBe('https://192.168.0.12:8080/app');
    } finally {
      vi.unstubAllGlobals();
    }
  });
});
