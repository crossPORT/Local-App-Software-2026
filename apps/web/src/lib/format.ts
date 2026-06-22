export function formatBytes(bytes: number): string {
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KiB`;
  }
  if (bytes < 1024 * 1024 * 1024) {
    const mib = bytes / (1024 * 1024);
    if (mib >= 100) {
      return `${mib.toFixed(0)} MiB`;
    }
    if (mib >= 10) {
      return `${mib.toFixed(1)} MiB`;
    }
    return `${mib.toFixed(2)} MiB`;
  }
  const gib = bytes / (1024 * 1024 * 1024);
  if (gib >= 100) {
    return `${gib.toFixed(0)} GiB`;
  }
  if (gib >= 10) {
    return `${gib.toFixed(1)} GiB`;
  }
  return `${gib.toFixed(2)} GiB`;
}

export function formatMbps(mbps: number): string {
  if (mbps >= 1024) {
    return `${(mbps / 1024).toFixed(2)} GiB/s`;
  }
  if (mbps >= 100) {
    return `${mbps.toFixed(0)} MiB/s`;
  }
  if (mbps >= 1) {
    return `${mbps.toFixed(1)} MiB/s`;
  }
  if (mbps > 0) {
    return `${Math.round(mbps * 1024)} KiB/s`;
  }
  return '—';
}

export function formatTransferDoneMessage(sent: boolean, mbps: number): string {
  if (mbps <= 0) {
    return sent ? 'Sent' : 'Received';
  }
  return `${sent ? 'Sent at' : 'Received at'} ${formatMbps(mbps)}`;
}

export function isTransferCompleteMessage(message: string): boolean {
  return (
    message.startsWith('Sent at')
    || message.startsWith('Received at')
    || message === 'Sent'
    || message === 'Received'
  );
}

export function formatTransferLiveMessage(sent: boolean, mbps: number): string {
  if (mbps <= 0) {
    return sent ? 'Sending…' : 'Receiving…';
  }
  return `${sent ? 'Sending at' : 'Receiving at'} ${formatMbps(mbps)}`;
}

/** Sender waiting on offer accept / ready — not payload yet. */
export function isOutboundHandshakeWait(statusMessage: string): boolean {
  return (
    statusMessage.includes('to accept') ||
    statusMessage.includes('Sending offer') ||
    statusMessage.includes('did not start receiving')
  );
}

/** Live/chart speed: measured rate, or display rate while a transfer is active. */
export function effectiveDisplayMbps(
  liveMbps: number,
  demoDisplayMibS: number,
  busy: boolean,
): number {
  if (liveMbps > 0) {
    return liveMbps;
  }
  if (busy && demoDisplayMibS > 0) {
    return demoDisplayMibS;
  }
  return 0;
}

export function formatChartAxisSpeed(mbps: number): string {
  if (mbps <= 0) {
    return '0';
  }
  return formatMbps(mbps);
}

export function formatSpeedFromBytes(bytes: number, seconds: number): string {
  if (seconds <= 0 || bytes <= 0) {
    return '—';
  }
  const mibPerSec = bytes / (1024 * 1024) / seconds;
  if (mibPerSec >= 1) {
    return formatMbps(mibPerSec);
  }
  return `${Math.round(bytes / 1024 / seconds)} KiB/s`;
}

export function receiveStatusLabel(status: string): string {
  switch (status) {
    case 'open':
      return 'Saves files automatically';
    case 'busy':
      return 'Not accepting files';
    default:
      return 'Asks before saving';
  }
}
