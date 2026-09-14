import { theme } from '../lib/theme';
import type { AppUiState } from '../lib/types';
import { effectiveDisplayMbps, isOutboundHandshakeWait } from '../lib/format';
import { ActivityMonitor } from './ActivityMonitor';
import { ConnectionLed, deriveLinkLed } from './ConnectionLed';

function systemMetaLine(otherSystems: number): string {
  if (otherSystems <= 0) {
    return '';
  }
  if (otherSystems === 1) {
    return '1 other system on this fabric';
  }
  return `${otherSystems} other systems on this fabric`;
}

interface ConnectionPanelProps {
  state: AppUiState;
  ledPulse: boolean;
  usbDescription: string;
  disconnectedHint?: string;
  children?: React.ReactNode;
}

export function ConnectionPanel({
  state,
  ledPulse,
  usbDescription,
  disconnectedHint = 'Plug in your USB cable, then click Connect this system below.',
  children,
}: ConnectionPanelProps) {
  const led = deriveLinkLed(state.usbConnected, state.fabricConnected, state.busy);
  const systemName = state.identity.display_name.trim() || usbDescription || 'This system';
  const connected = state.usbConnected && state.fabricConnected;
  const chartMbps =
    state.busy && !isOutboundHandshakeWait(state.statusMessage)
      ? effectiveDisplayMbps(
          Math.max(state.fabricActivityMbps, state.liveMbps),
          state.boothDisplayMibS,
          true,
          state.bytesDone,
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
        This system
      </div>
      <div className="system-name-row">
        <ConnectionLed state={led} pulseOn={ledPulse} />
        <span className="connection-device" style={{ color: theme.usbInk }}>
          {systemName}
        </span>
      </div>
      {connected ? (
        <>
          {state.fabricDevicesSeen > 1 && (
            <div className="connection-meta" style={{ color: theme.usbMuted }}>
              {systemMetaLine(state.fabricDevicesSeen - 1)}
            </div>
          )}
          <ActivityMonitor
            visible
            compact
            persistHistory
            sessionPulse={state.fabricActivitySeq}
            transferMbps={chartMbps}
            resultMbps={state.resultMbps}
            scaleFloorMbps={scaleFloorMbps}
          />
        </>
      ) : (
        <p className="connection-hint" style={{ color: theme.usbMuted }}>
          {state.fabricDevicesSeen > 0
            ? 'USB cable detected — click Connect this system and pick it in the browser dialog.'
            : disconnectedHint}
        </p>
      )}
      {children}
    </section>
  );
}
