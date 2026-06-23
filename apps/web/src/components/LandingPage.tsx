import { useCallback, useEffect, useRef, useState } from 'react';
import QRCode from 'qrcode';
import { fetchBoothNetworkInfo } from '../lib/booth_network';
import {
  boothOriginFromIp,
  buildPwaAppUrl,
  isLocalHostname,
  readSavedBoothOrigin,
  resolvePwaAppUrl,
  saveBoothOrigin,
} from '../lib/pwa_url';
import { theme } from '../lib/theme';

export function LandingPage() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [pwaUrl, setPwaUrl] = useState(() => resolvePwaAppUrl());
  const [lanIps, setLanIps] = useState<string[]>([]);
  const [selectedIp, setSelectedIp] = useState('');
  const [port, setPort] = useState(() => window.location.port || '8080');
  const [localhostWarning, setLocalhostWarning] = useState(() =>
    isLocalHostname(window.location.hostname),
  );

  useEffect(() => {
    void (async () => {
      const info = await fetchBoothNetworkInfo();
      if (!info) {
        return;
      }
      setLanIps(info.ips);
      setPort(String(info.port));
      const saved = readSavedBoothOrigin();
      if (saved) {
        setPwaUrl(buildPwaAppUrl(saved));
        return;
      }
      const first = info.ips[0];
      if (first) {
        setSelectedIp(first);
        const origin = boothOriginFromIp(first, info.port, info.protocol);
        setPwaUrl(buildPwaAppUrl(origin));
      }
    })();
  }, []);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !pwaUrl) {
      return;
    }
    void QRCode.toCanvas(canvas, pwaUrl, {
      width: 280,
      margin: 2,
      color: { dark: '#0f1620', light: '#ffffff' },
    });
  }, [pwaUrl]);

  const applyLanOrigin = useCallback(() => {
    if (!selectedIp) {
      return;
    }
    const protocol = window.isSecureContext ? 'https:' : window.location.protocol;
    const origin = boothOriginFromIp(selectedIp, port, protocol);
    saveBoothOrigin(origin);
    setPwaUrl(buildPwaAppUrl(origin));
    setLocalhostWarning(false);
  }, [port, selectedIp]);

  const openApp = useCallback(() => {
    window.location.assign('/app');
  }, []);

  return (
    <div className="landing-page">
      <div className="landing-card">
        <div className="landing-brand">
          <span className="landing-icon" aria-hidden>
            🚀
          </span>
          <h1>RocketBox App</h1>
          <p className="landing-tagline" style={{ color: theme.muted }}>
            Scan to open the transfer app on your phone or tablet
          </p>
        </div>

        <div className="landing-qr-wrap">
          <canvas ref={canvasRef} className="landing-qr" aria-label="QR code to open RocketBox App" />
        </div>

        <a className="landing-url" href={pwaUrl} style={{ color: theme.accent }}>
          {pwaUrl}
        </a>

        {localhostWarning && (
          <div className="landing-warning" style={{ borderColor: theme.warn, color: theme.warn }}>
            <strong>Phones cannot use localhost.</strong> Pick your booth PC&apos;s LAN address below so
            the QR code works on other devices.
          </div>
        )}

        {lanIps.length > 0 && (
          <div className="landing-lan-picker">
            <label>
              Booth LAN address
              <div className="landing-lan-row">
                <select value={selectedIp} onChange={(e) => setSelectedIp(e.target.value)}>
                  {lanIps.map((ip) => (
                    <option key={ip} value={ip}>
                      {ip}
                    </option>
                  ))}
                </select>
                <span className="landing-port-label">:</span>
                <input
                  type="text"
                  inputMode="numeric"
                  value={port}
                  onChange={(e) => setPort(e.target.value)}
                  aria-label="Port"
                  className="landing-port-input"
                />
                <button type="button" className="primary" onClick={applyLanOrigin}>
                  Update QR
                </button>
              </div>
            </label>
          </div>
        )}

        <ol className="landing-steps" style={{ color: theme.muted }}>
          <li>Scan the QR code with your phone camera</li>
          <li>Accept the HTTPS certificate warning if prompted</li>
          <li>Plug in your USB cable and tap <strong>Connect USB</strong></li>
          <li>Optional: add to home screen for a full-screen app</li>
        </ol>

        <div className="landing-actions">
          <button type="button" className="primary landing-open-btn" onClick={openApp}>
            Open app on this device
          </button>
        </div>
      </div>
    </div>
  );
}
