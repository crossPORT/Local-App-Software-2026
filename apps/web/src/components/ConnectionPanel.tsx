import { theme } from '../lib/theme';
import type { AppUiState } from '../lib/types';
import { effectiveDisplayMbps, isOutboundHandshakeWait } from '../lib/format';
import { ActivityMonitor } from './ActivityMonitor';

function deviceMetaLine(devicesSeen: number, connected: boolean): string {
  if (devicesSeen === 0) {
    return 'No devices detected';
  }
  if (devicesSeen === 1) {
    return 'Connected';
  }
  if (connected) {
    return `${devicesSeen}-port crossport switching block detected`;
  }
  return `${devicesSeen} devices — pick your cable in the USB dialog`;
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
  const connected = state.usbConnected;
  const chartMbps =
    state.busy && !isOutboundHandshakeWait(state.statusMessage)
      ? effectiveDisplayMbps(
          Math.max(state.usbActivityMbps, state.liveMbps),
          state.displayRateMibS,
          true,
          state.bytesDone,
        )
      : 0;
  const scaleFloorMbps =
    state.busy &&
    !isOutboundHandshakeWait(state.statusMessage) &&
    state.identity.display_rate_mib_s > 0
      ? Math.max(state.displayRateMibS, state.identity.display_rate_mib_s)
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
          {state.devicesSeen > 1 && (
            <div className="connection-meta" style={{ color: theme.muted }}>
              {deviceMetaLine(state.devicesSeen, true)}
            </div>
          )}
          <ActivityMonitor
            visible
            compact
            persistHistory
            sessionPulse={state.usbActivitySeq}
            transferMbps={chartMbps}
            resultMbps={state.resultMbps}
            scaleFloorMbps={scaleFloorMbps}
          />
        </>
      ) : (
        <p className="connection-hint" style={{ color: theme.muted }}>
          {state.devicesSeen > 0
            ? 'USB cable detected — click Connect USB and pick it in the browser dialog.'
            : disconnectedHint}
        </p>
      )}
      {children}
    </section>
  );
}
