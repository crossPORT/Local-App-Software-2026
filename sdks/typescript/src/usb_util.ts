export function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

export function isUsbTransferContentionError(err: unknown): boolean {
  const message = (err as Error)?.message ?? '';
  return (
    message.includes('operation that changes the device state is in progress') ||
    message.includes('not part of a claimed') ||
    message.includes('A transfer error has occurred')
  );
}

export function isBenignUsbListenError(err: unknown): boolean {
  const message = (err as Error)?.message ?? '';
  return (
    message === 'session header timeout' ||
    message.includes('transfer error') ||
    message.includes('transferIn returned no data') ||
    message.includes('transferIn failed')
  );
}

export function mergeBuffers(parts: Uint8Array[], totalLength: number): Uint8Array {
  const out = new Uint8Array(totalLength);
  let offset = 0;
  for (const part of parts) {
    out.set(part, offset);
    offset += part.length;
  }
  return out;
}
