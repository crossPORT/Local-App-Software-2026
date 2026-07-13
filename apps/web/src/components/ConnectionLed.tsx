import { theme, type LinkLed } from '../lib/theme';
import { webUsbBlockedReason } from '@rocketbox/sdk';

interface ConnectionLedProps {
  state: LinkLed;
  pulseOn?: boolean;
  onClick?: () => void;
}

const colours: Record<LinkLed, { bg: string; title: string }> = {
  offline: { bg: theme.error, title: 'Not connected' },
  announcing: { bg: theme.warn, title: 'Connecting…' },
  connected: { bg: theme.ok, title: 'Connected — click to announce' },
  transferring: { bg: theme.pulseGreen, title: 'Transfer in progress — click to announce' },
};

export function ConnectionLed({ state, pulseOn = false, onClick }: ConnectionLedProps) {
  const { bg, title } = colours[state];
  const background = state === 'transferring' && pulseOn ? theme.ok : bg;
  const clickable = Boolean(onClick) && (state === 'connected' || state === 'transferring');
  return (
    <span
      className="connection-led"
      title={title}
      style={{ backgroundColor: background, cursor: clickable ? 'pointer' : undefined }}
      aria-label={title}
      role={clickable ? 'button' : undefined}
      tabIndex={clickable ? 0 : undefined}
      onClick={clickable ? onClick : undefined}
      onKeyDown={
        clickable
          ? (e) => {
              if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                onClick?.();
              }
            }
          : undefined
      }
    />
  );
}

export function deriveLinkLed(usbConnected: boolean, busy: boolean): LinkLed {
  if (busy && usbConnected) {
    return 'transferring';
  }
  if (usbConnected) {
    return 'connected';
  }
  return 'offline';
}

export function statusLine(
  usbConnected: boolean,
  devicesSeen: number,
  _portIndex: number,
  busy: boolean,
  waitingForPartner: boolean,
  statusMessage: string,
  peersConfigured: boolean,
): { text: string; colour: string } {
  if ((busy || waitingForPartner) && statusMessage) {
    return { text: statusMessage, colour: theme.accent };
  }
  if (usbConnected && !peersConfigured) {
    return { text: 'USB connected — waiting for peers', colour: theme.warn };
  }
  if (usbConnected) {
    return { text: 'USB connected', colour: theme.ok };
  }
  if (webUsbBlockedReason()) {
    const reason = webUsbBlockedReason()!;
    return {
      text: reason,
      colour: theme.error,
    };
  }
  if (devicesSeen === 0) {
    return { text: 'Plug in your USB cable, then connect below', colour: theme.warn };
  }
  return { text: 'Click Connect USB below', colour: theme.warn };
}
