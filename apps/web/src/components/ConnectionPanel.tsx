import { theme } from '../lib/theme';
import type { AppUiState } from '../lib/types';
import { effectiveDisplayMbps, isOutboundHandshakeWait } from '../lib/format';
import { ActivityMonitor } from './ActivityMonitor';

function deviceMetaLine(portIndex: number, devicesSeen: number): string {
  if (devicesSeen === 0) {
    return `Port ${portIndex} · no devices detected`;
  }
  if (devicesSeen === 1) {
    return `Port ${portIndex} · 1 device connected`;
  }
  return `Port ${portIndex} · ${devicesSeen} devices connected`;
}

interface ConnectionPanelProps {
  state: AppUiState;
  usbDescription: string;
  disconnectedHint?: string;
  children?: React.ReactNode;
}

export function ConnectionPanel({
  state,
  usbDescription,
  disconnectedHint = 'Plug in your USB cable, then click Connect USB below.',
  children,
}: ConnectionPanelProps) {
  const connected = state.usbConnected && state.fabricConnected;
  const chartMbps =
    state.busy && !isOutboundHandshakeWait(state.statusMessage)
      ? effectiveDisplayMbps(
          Math.max(state.fabricActivityMbps, state.liveMbps),
          state.boothDisplayMibS,
          true,
        )
      : 0;
  const scaleFloorMbps =
    state.busy &&
    !isOutboundHandshakeWait(state.statusMessage) &&
    state.identity.booth_display_mib_s > 0
      ? Math.max(state.boothDisplayMibS, state.identity.booth_display_mib_s)
      : chartMbps > 0
        ? chartMbps
        : 0;

  return (
    <section className="connection-panel">
      <div className="section-label" style={{ color: theme.accent }}>
        USB device
      </div>
      {connected ? (
        <>
          <div className="connection-device" style={{ color: theme.text }}>
            {usbDescription || 'USB cable connected'}
          </div>
          <div className="connection-meta" style={{ color: theme.muted }}>
            {deviceMetaLine(state.portIndex, state.fabricDevicesSeen)}
          </div>
          <ActivityMonitor
            visible
            compact
            persistHistory
            sessionPulse={state.fabricActivitySeq}
            transferMbps={chartMbps}
            scaleFloorMbps={scaleFloorMbps}
          />
        </>
      ) : (
        <p className="connection-hint" style={{ color: theme.muted }}>
          {state.fabricDevicesSeen > 0
            ? 'USB cable detected — click Connect USB and pick it in the browser dialog.'
            : disconnectedHint}
        </p>
      )}
      {children}
    </section>
  );
}
