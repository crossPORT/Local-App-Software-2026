import { theme, type LinkLed } from '../lib/theme';

interface ConnectionLedProps {
  state: LinkLed;
  pulseOn?: boolean;
}

const colours: Record<LinkLed, { bg: string; title: string }> = {
  offline: { bg: theme.error, title: 'Not connected' },
  announcing: { bg: theme.warn, title: 'Connecting…' },
  connected: { bg: theme.ok, title: 'USB connected' },
  transferring: { bg: theme.pulseGreen, title: 'Transfer in progress' },
};

export function ConnectionLed({ state, pulseOn = false }: ConnectionLedProps) {
  const { bg, title } = colours[state];
  const background = state === 'transferring' && pulseOn ? theme.ok : bg;
  return (
    <span
      className="connection-led"
      title={title}
      style={{ backgroundColor: background }}
      aria-label={title}
    />
  );
}

export function deriveLinkLed(
  usbConnected: boolean,
  fabricConnected: boolean,
  busy: boolean,
): LinkLed {
  if (busy && fabricConnected) {
    return 'transferring';
  }
  if (fabricConnected) {
    return 'connected';
  }
  if (usbConnected) {
    return 'announcing';
  }
  return 'offline';
}

export function statusLine(
  _usbConnected: boolean,
  fabricConnected: boolean,
  fabricDevicesSeen: number,
  _portIndex: number,
  busy: boolean,
  waitingForPartner: boolean,
  statusMessage: string,
  peersConfigured: boolean,
): { text: string; colour: string } {
  if ((busy || waitingForPartner) && statusMessage) {
    return { text: statusMessage, colour: theme.accent };
  }
  if (fabricConnected && !peersConfigured) {
    return { text: 'USB connected — waiting for peers', colour: theme.warn };
  }
  if (fabricConnected) {
    return { text: 'USB connected', colour: theme.ok };
  }
  if (!navigator.usb) {
    return {
      text: 'Use Chrome on this computer — other browsers cannot connect USB',
      colour: theme.error,
    };
  }
  if (fabricDevicesSeen === 0) {
    return { text: 'Plug in your USB cable, then connect below', colour: theme.warn };
  }
  if (!fabricConnected) {
    return { text: 'Click Connect USB below', colour: theme.warn };
  }
  return { text: 'USB allowed — click Connect USB below', colour: theme.warn };
}
