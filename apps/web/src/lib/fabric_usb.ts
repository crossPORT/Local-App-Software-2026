import {
  rememberFabricPortForSerial,
  resolveFabricPortIndex,
  sortFabricDevicesBySerial,
} from './fabric_port';
import {
  CHUNK_SIZE,
  EP_IN,
  EP_OUT,
  HEADER_SIZE,
  INTERFACE_NUMBER,
  PRODUCT_ID,
  VENDOR_ID,
  buildHeader,
  concatChunks,
  parseHeader,
} from './fabric_protocol';
import {
  type FabricSessionMessage,
  parseSessionPayload,
  serializeSessionMessage,
} from './fabric_session';
import type { FabricTransport } from './fabric_transport';
import { MAX_SESSION_FILE_BYTES } from './usb_constants';
import { webUsbBlockedReason } from './webusb_env';

const SELECTED_SERIAL_KEY = 'rocketbox_usb_serial';
const SESSION_SEND_TIMEOUT_MS = 2500;
const USB_OPEN_ATTEMPTS = 3;

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

function legacySerialKey(portIndex: number): string {
  return `rocketbox_serial_port${portIndex}`;
}

function getSavedSerial(): string | null {
  const primary = sessionStorage.getItem(SELECTED_SERIAL_KEY);
  if (primary) {
    return primary;
  }
  for (let port = 0; port <= 1; port += 1) {
    const legacy = sessionStorage.getItem(legacySerialKey(port));
    if (legacy) {
      sessionStorage.setItem(SELECTED_SERIAL_KEY, legacy);
      return legacy;
    }
  }
  return null;
}

function clearSavedSerial(): void {
  sessionStorage.removeItem(SELECTED_SERIAL_KEY);
  for (let port = 0; port <= 1; port += 1) {
    sessionStorage.removeItem(legacySerialKey(port));
  }
}

export class FabricUsbError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'FabricUsbError';
  }
}

function mergeBuffers(parts: Uint8Array[], totalLength: number): Uint8Array {
  const out = new Uint8Array(totalLength);
  let offset = 0;
  for (const part of parts) {
    out.set(part, offset);
    offset += part.length;
  }
  return out;
}

async function transferInExact(device: USBDevice, endpoint: number, byteCount: number): Promise<Uint8Array> {
  const parts: Uint8Array[] = [];
  let received = 0;
  while (received < byteCount) {
    const result = await device.transferIn(endpoint, byteCount - received);
    if (result.status !== 'ok') {
      throw new FabricUsbError(`transferIn failed: ${result.status}`);
    }
    const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
    if (chunk.length === 0) {
      throw new FabricUsbError('transferIn returned no data');
    }
    parts.push(chunk);
    received += chunk.length;
  }
  return mergeBuffers(parts, byteCount);
}

async function transferOutChecked(device: USBDevice, endpoint: number, data: BufferSource): Promise<void> {
  const result = await device.transferOut(endpoint, data);
  if (result.status !== 'ok') {
    throw new FabricUsbError(`transferOut failed: ${result.status}`);
  }
}

async function transferOutWithTimeout(
  device: USBDevice,
  endpoint: number,
  data: BufferSource,
  timeoutMs: number,
): Promise<void> {
  await Promise.race([
    transferOutChecked(device, endpoint, data),
    new Promise<never>((_, reject) => {
      window.setTimeout(
        () => reject(new FabricUsbError(`transferOut timed out after ${timeoutMs}ms`)),
        timeoutMs,
      );
    }),
  ]);
}

async function sendSessionMessageWithTimeout(
  device: USBDevice,
  bytes: Uint8Array,
  timeoutMs: number,
): Promise<void> {
  await transferOutWithTimeout(device, EP_OUT, buildHeader(bytes.length, ''), timeoutMs);
  await transferOutWithTimeout(device, EP_OUT, new Uint8Array(bytes) as unknown as BufferSource, timeoutMs);
}

function isFabricDevice(device: USBDevice): boolean {
  return device.vendorId === VENDOR_ID && device.productId === PRODUCT_ID;
}

function sortFabricDevices(devices: USBDevice[]): USBDevice[] {
  return sortFabricDevicesBySerial(devices);
}

async function resetDeviceHandle(device: USBDevice): Promise<void> {
  if (!device.opened) {
    return;
  }
  try {
    await device.releaseInterface(INTERFACE_NUMBER);
  } catch {
    // Interface may already be released.
  }
  try {
    await device.close();
  } catch {
    // Best effort — continue and try open again.
  }
}

function mapUsbOpenError(err: unknown): FabricUsbError {
  const message = (err as Error)?.message ?? String(err);
  if (message.includes('disconnected')) {
    return new FabricUsbError(
      'Could not reach the USB device — click Reconnect USB. If it keeps failing, power-cycle the RocketBox.',
    );
  }
  if (message.includes('Access denied')) {
    return new FabricUsbError(
      'USB access denied — close other browser tabs using this cable, then click Connect USB.',
    );
  }
  return new FabricUsbError(`Could not open USB device — ${message}`);
}

async function openFabricDevice(device: USBDevice): Promise<void> {
  if (!device.opened) {
    try {
      await device.open();
    } catch (err) {
      throw mapUsbOpenError(err);
    }
  }
  if (device.configuration == null) {
    await device.selectConfiguration(1);
  }
  try {
    await device.claimInterface(INTERFACE_NUMBER);
  } catch (err) {
    const message = (err as Error).message ?? '';
    try {
      await device.releaseInterface(INTERFACE_NUMBER);
      await device.claimInterface(INTERFACE_NUMBER);
    } catch {
      if (message.includes('claim') || message.includes('busy') || message.includes('Access')) {
        throw new FabricUsbError(
          'USB interface is busy — close other RocketBox App tabs or native apps using this cable, then try Forget USB device.',
        );
      }
      throw new FabricUsbError(`Could not claim USB interface — ${message}`);
    }
  }
}

async function openFabricDeviceWithRetry(device: USBDevice, attempts = USB_OPEN_ATTEMPTS): Promise<void> {
  let lastErr: unknown;
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    if (attempt > 0) {
      await sleep(400 * attempt);
      await resetDeviceHandle(device);
    }
    try {
      await openFabricDevice(device);
      return;
    } catch (err) {
      lastErr = err;
    }
  }
  throw lastErr instanceof Error ? lastErr : mapUsbOpenError(lastErr);
}

function rememberSerial(device: USBDevice): void {
  if (device.serialNumber) {
    sessionStorage.setItem(SELECTED_SERIAL_KEY, device.serialNumber);
  }
}

/** Match by saved serial from the native picker; never guess by sort index. */
function findPairedDevice(devices: USBDevice[]): USBDevice | null {
  const sorted = sortFabricDevices(devices);
  if (sorted.length === 0) {
    return null;
  }
  const savedSerial = getSavedSerial();
  if (savedSerial) {
    const match = sorted.find((d) => d.serialNumber === savedSerial);
    if (match) {
      return match;
    }
  }
  if (sorted.length === 1) {
    return sorted[0];
  }
  return null;
}

export class FabricUsbSession implements FabricTransport {
  device: USBDevice | null = null;
  private usbTail: Promise<void> = Promise.resolve();
  private resolvedFabricPortIndex = 0;

  constructor(private readonly portHint = 0) {}

  getFabricPortIndex(): number {
    return this.resolvedFabricPortIndex;
  }

  private async refreshResolvedPortIndex(): Promise<void> {
    if (!this.device || !navigator.usb) {
      return;
    }
    const devices = (await navigator.usb.getDevices()).filter(isFabricDevice);
    this.resolvedFabricPortIndex = resolveFabricPortIndex(this.device, devices, this.portHint);
    rememberFabricPortForSerial(this.device.serialNumber, this.resolvedFabricPortIndex);
    console.log(
      `[RocketBox] fabric port ${this.resolvedFabricPortIndex} (serial ${this.device.serialNumber ?? 'unknown'})`,
    );
  }

  /** Serialize all USB operations — this FPGA device requires exclusive access
   *  (no concurrent IN+OUT), matching the native app's single usb_mutex. */
  private runUsb<T>(label: string, fn: () => Promise<T>): Promise<T> {
    console.debug(`[FabricUSB] runUsb queue: ${label}`);
    const run = this.usbTail.then(() => {
      console.debug(`[FabricUSB] runUsb start: ${label}`);
      return fn();
    });
    this.usbTail = run.then(
      () => { console.debug(`[FabricUSB] runUsb done: ${label}`); },
      () => { console.debug(`[FabricUSB] runUsb fail: ${label}`); },
    );
    return run;
  }

  /** Wait until all queued USB operations finish. */
  async waitForIdle(): Promise<void> {
    await this.usbTail;
  }

  /** Stop a stuck session transferIn and drain the queue before sending payload OUT. */
  async prepareForPayloadSend(): Promise<void> {
    await this.waitForIdle();
    await this.abortStuckTransfer();
  }

  /** Release/reclaim aborts a stuck WebUSB transferIn (no native timeout). */
  private async abortStuckTransfer(): Promise<void> {
    const device = this.device;
    if (!device?.opened) {
      return;
    }
    try {
      await device.releaseInterface(INTERFACE_NUMBER);
      await device.claimInterface(INTERFACE_NUMBER);
    } catch {
      // best effort
    }
  }

  get connected(): boolean {
    return this.device != null && this.device.opened;
  }

  static async countFabricDevices(): Promise<number> {
    if (!navigator.usb) {
      return 0;
    }
    return (await navigator.usb.getDevices()).filter(isFabricDevice).length;
  }

  static hasSavedSerial(): boolean {
    return getSavedSerial() != null;
  }

  async connect(): Promise<string> {
    const blocked = webUsbBlockedReason();
    if (blocked) {
      throw new FabricUsbError(blocked);
    }
    await this.disconnect();
    let device: USBDevice;
    try {
      device = await navigator.usb.requestDevice({
        filters: [{ vendorId: VENDOR_ID, productId: PRODUCT_ID }],
      });
    } catch (err) {
      if (err instanceof DOMException && (err.name === 'NotFoundError' || err.message.includes('No device selected'))) {
        throw err;
      }
      throw mapUsbOpenError(err);
    }
    await resetDeviceHandle(device);
    await openFabricDeviceWithRetry(device);
    this.device = device;
    rememberSerial(device);
    await this.refreshResolvedPortIndex();
    return this.describeDevice();
  }

  async reconnectKnown(): Promise<string> {
    if (!navigator.usb) {
      throw new FabricUsbError('WebUSB unavailable');
    }
    if (!FabricUsbSession.hasSavedSerial()) {
      throw new FabricUsbError('No saved cable for this window — click Connect USB');
    }
    await this.disconnect();
    const devices = (await navigator.usb.getDevices()).filter(isFabricDevice);
    const device = findPairedDevice(devices);
    if (!device) {
      throw new FabricUsbError(
        'Previously paired device not found — click Connect USB and pick a cable',
      );
    }
    await resetDeviceHandle(device);
    try {
      await openFabricDeviceWithRetry(device);
    } catch (err) {
      throw err instanceof FabricUsbError
        ? err
        : new FabricUsbError('Saved device handle is stale — click Reconnect USB or power-cycle the RocketBox');
    }
    this.device = device;
    rememberSerial(device);
    await this.refreshResolvedPortIndex();
    return this.describeDevice();
  }

  describeDevice(): string {
    if (!this.device) {
      return '';
    }
    return [
      this.device.productName || 'SLS USB DEVICE',
      this.device.serialNumber || '(no serial)',
    ].join(' · ');
  }

  async disconnect(): Promise<void> {
    if (!this.device?.opened) {
      this.device = null;
      return;
    }
    try {
      await this.device.releaseInterface(INTERFACE_NUMBER);
    } catch {
      // Interface may already be released after a failed transfer.
    }
    try {
      await this.device.close();
    } finally {
      this.device = null;
    }
  }

  /** Release this window's USB permission so Chrome shows the picker again. */
  async forgetThisDevice(): Promise<void> {
    const savedSerial = getSavedSerial();
    await this.disconnect();
    if (!navigator.usb) {
      return;
    }
    const devices = (await navigator.usb.getDevices()).filter(isFabricDevice);
    for (const device of devices) {
      if (savedSerial && device.serialNumber !== savedSerial) {
        continue;
      }
      try {
        if (device.opened) {
          try {
            await device.releaseInterface(INTERFACE_NUMBER);
          } catch {
            /* best effort */
          }
          await device.close();
        }
        await device.forget();
      } catch {
        // Best effort — device may already be gone.
      }
    }
    clearSavedSerial();
  }

  /** Abort a blocked transferIn by closing and re-opening the device. */
  async resetConnection(): Promise<string> {
    if (this.device?.serialNumber) {
      rememberSerial(this.device);
    } else if (!getSavedSerial()) {
      throw new FabricUsbError('USB not connected — click Connect USB');
    }
    await this.disconnect();
    return this.reconnectKnown();
  }

  ownsDevice(usbDevice: USBDevice): boolean {
    return this.device === usbDevice;
  }

  markDisconnected(): void {
    this.device = null;
  }

  async sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    await this.runUsb('sendBytes', async () => {
      if (!this.device) {
        throw new FabricUsbError('USB not connected');
      }
      console.debug(`[FabricUSB] sendBytes: header (${payload.length} bytes payload)`);
      await transferOutChecked(this.device, EP_OUT, buildHeader(payload.length, filename));
      onProgress?.(0, payload.length);

      let offset = 0;
      while (offset < payload.length) {
        const chunk = new Uint8Array(CHUNK_SIZE);
        const n = Math.min(payload.length - offset, CHUNK_SIZE);
        chunk.set(payload.subarray(offset, offset + n));
        console.debug(`[FabricUSB] sendBytes: chunk ${offset}/${payload.length} (${CHUNK_SIZE} wire bytes)`);
        await transferOutChecked(this.device, EP_OUT, chunk);
        offset += n;
        onProgress?.(offset, payload.length);
      }
      console.debug('[FabricUSB] sendBytes: complete');
    });
  }

  async receiveHeader(): Promise<{ fileSize: number; filename: string }> {
    return this.runUsb('receiveHeader', async () => {
      if (!this.device) {
        throw new FabricUsbError('USB not connected');
      }
      const headerBuf = await transferInExact(this.device, EP_IN, HEADER_SIZE);
      return parseHeader(headerBuf);
    });
  }

  async receivePayload(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    return this.runUsb('receivePayload', async () => {
      if (!this.device) {
        throw new FabricUsbError('USB not connected');
      }
      const parts: Uint8Array[] = [];
      let received = 0;
      while (received < fileSize) {
        const result = await this.device.transferIn(EP_IN, CHUNK_SIZE);
        if (result.status !== 'ok') {
          throw new FabricUsbError(`Payload transferIn failed: ${result.status}`);
        }
        const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
        const take = Math.min(chunk.length, fileSize - received);
        parts.push(chunk.subarray(0, take));
        received += take;
        onProgress?.(received, fileSize);
      }
      return concatChunks(parts, fileSize);
    });
  }

  async discardPayload(fileSize: number): Promise<void> {
    await this.receivePayload(fileSize);
  }

  async receiveBytes(onProgress?: (done: number, total: number) => void): Promise<Uint8Array> {
    const { fileSize } = await this.receiveHeader();
    return this.receivePayload(fileSize, onProgress);
  }

  async sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    const bytes = serializeSessionMessage(message);
    await this.runUsb('sendSession', async () => {
      if (!this.device) {
        throw new FabricUsbError('USB not connected');
      }
      await sendSessionMessageWithTimeout(this.device, bytes, SESSION_SEND_TIMEOUT_MS);
    });
  }

  /** Session listener poll — returns null on header timeout (matches wx SessionListener). */
  async tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    if (!this.device) {
      return null;
    }

    return this.runUsb('tryReceiveSession', async () => {
      if (!this.device) {
        return null;
      }

      let timeoutId: ReturnType<typeof setTimeout> | undefined;
      let timedOut = false;
      try {
        console.debug('[FabricUSB] tryReceiveSession: waiting for header…');
        const headerBuf = await Promise.race([
          transferInExact(this.device, EP_IN, HEADER_SIZE),
          new Promise<never>((_, reject) => {
            timeoutId = window.setTimeout(() => {
              timedOut = true;
              reject(new FabricUsbError('session header timeout'));
            }, headerTimeoutMs);
          }),
        ]);
        if (timeoutId !== undefined) {
          window.clearTimeout(timeoutId);
        }

        const { fileSize } = parseHeader(headerBuf);
        console.log(`[RocketBox] header received, fileSize=${fileSize}`);
        if (fileSize === 0 || fileSize > MAX_SESSION_FILE_BYTES) {
          if (fileSize > MAX_SESSION_FILE_BYTES) {
            await this.receivePayloadWithinLock(fileSize).catch(() => {});
          }
          return null;
        }
        const data = await this.receivePayloadWithinLock(fileSize);
        const parsed = parseSessionPayload(data);
        if (!parsed) {
          console.warn('[RocketBox] parseSessionPayload returned null for', fileSize, 'bytes');
        }
        return parsed;
      } catch (err) {
        if (timeoutId !== undefined) {
          window.clearTimeout(timeoutId);
        }
        if (timedOut || (err as FabricUsbError).message === 'session header timeout') {
          console.debug('[FabricUSB] tryReceiveSession: timeout (normal idle)');
        } else {
          console.debug('[FabricUSB] tryReceiveSession: no data:', (err as Error).message);
        }
        await this.abortStuckTransfer();
        return null;
      }
    });
  }

  private async receivePayloadWithinLock(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    if (!this.device) {
      throw new FabricUsbError('USB not connected');
    }
    const parts: Uint8Array[] = [];
    let received = 0;
    while (received < fileSize) {
      const result = await this.device.transferIn(EP_IN, CHUNK_SIZE);
      if (result.status !== 'ok') {
        throw new FabricUsbError(`Payload transferIn failed: ${result.status}`);
      }
      const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
      const take = Math.min(chunk.length, fileSize - received);
      parts.push(chunk.subarray(0, take));
      received += take;
      onProgress?.(received, fileSize);
    }
    return concatChunks(parts, fileSize);
  }

  async receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    if (!this.device) {
      throw new FabricUsbError('USB not connected');
    }

    return this.runUsb('receiveFileTransfer', async () => {
      if (!this.device) {
        throw new FabricUsbError('USB not connected');
      }

      const deadline = Date.now() + headerTimeoutMs;
      let skipped = 0;
      const maxSkips = 16;

      while (Date.now() < deadline && skipped < maxSkips) {
        const remaining = Math.max(1, deadline - Date.now());
        let timeoutId: ReturnType<typeof setTimeout> | undefined;
        let timedOut = false;
        try {
          const headerBuf = await Promise.race([
            transferInExact(this.device, EP_IN, HEADER_SIZE),
            new Promise<never>((_, reject) => {
              timeoutId = window.setTimeout(() => {
                timedOut = true;
                reject(new FabricUsbError('payload header timeout'));
              }, remaining);
            }),
          ]);
          if (timeoutId !== undefined) {
            window.clearTimeout(timeoutId);
          }

          const header = parseHeader(headerBuf);
          const { fileSize, filename } = header;
          if (fileSize === 0) {
            skipped += 1;
            continue;
          }

          const trackProgress = expectedBytes > 0 && fileSize === expectedBytes;
          const payload = await this.receivePayloadWithinLock(
            fileSize,
            trackProgress ? onProgress : undefined,
          );

          if (fileSize <= MAX_SESSION_FILE_BYTES) {
            const parsed = parseSessionPayload(payload);
            if (parsed) {
              console.warn(
                '[FabricUSB] skipping stray session frame while waiting for payload',
                parsed.kind,
                fileSize,
              );
              skipped += 1;
              continue;
            }
            if (expectedBytes > 0 && fileSize !== expectedBytes) {
              console.warn(
                '[FabricUSB] skipping unexpected frame size',
                fileSize,
                `(expected ${expectedBytes})`,
              );
              skipped += 1;
              continue;
            }
          } else if (expectedBytes > 0 && fileSize !== expectedBytes) {
            throw new FabricUsbError(
              `Expected ${expectedBytes} byte payload but header announced ${fileSize}`,
            );
          }

          return {
            data: payload,
            filename: filename || 'download.bin',
          };
        } catch (err) {
          if (timeoutId !== undefined) {
            window.clearTimeout(timeoutId);
          }
          if (timedOut) {
            await this.abortStuckTransfer();
          }
          throw err;
        }
      }

      throw new FabricUsbError('payload header timeout');
    });
  }
}

export async function readFilePayload(file: File): Promise<Uint8Array> {
  const buffer = await file.arrayBuffer();
  return new Uint8Array(buffer);
}
