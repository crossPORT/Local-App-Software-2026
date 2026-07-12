/** Timed WebUSB bulk helpers for ROCKETBX data plane (half-duplex). */

export function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

export function isUsbContention(err: unknown): boolean {
  const m = (err as Error)?.message ?? '';
  return (
    m.includes('operation that changes the device state is in progress') ||
    m.includes('not part of a claimed') ||
    m.includes('A transfer error has occurred')
  );
}

export function isBenignListenError(err: unknown): boolean {
  const m = (err as Error)?.message ?? '';
  return (
    m.includes('The transfer was cancelled') ||
    m.includes('The device was disconnected') ||
    (err as Error)?.name === 'NetworkError'
  );
}

export async function transferInWithTimeout(
  device: USBDevice,
  epIn: number,
  length: number,
  timeoutMs: number,
): Promise<USBInTransferResult | null> {
  let settled = false;
  const read = device.transferIn(epIn, length).then(
    (r) => {
      settled = true;
      return r;
    },
    () => {
      settled = true;
      return null;
    },
  );
  const timed = await Promise.race([
    read,
    sleep(timeoutMs).then(() => null),
  ]);
  if (timed) return timed;
  if (!settled) {
    /* leave outstanding read; caller may reuse */
  }
  return null;
}

export async function transferOutWithTimeout(
  device: USBDevice,
  epOut: number,
  data: BufferSource,
  timeoutMs: number,
): Promise<void> {
  const result = await Promise.race([
    device.transferOut(epOut, data),
    sleep(timeoutMs).then(() => {
      throw new Error('USB OUT timeout');
    }),
  ]);
  if (!result || result.status !== 'ok') {
    throw new Error(`USB OUT failed status=${result?.status ?? 'timeout'}`);
  }
}

export async function transferOutWithRetry(
  device: USBDevice,
  epOut: number,
  data: BufferSource,
  timeoutMs: number,
  recover: () => Promise<void>,
): Promise<void> {
  try {
    await transferOutWithTimeout(device, epOut, data, timeoutMs);
  } catch (err) {
    if (!isUsbContention(err)) throw err;
    await recover();
    await transferOutWithTimeout(device, epOut, data, timeoutMs);
  }
}
