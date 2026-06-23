import { boothLog } from './booth_log';
import { FabricUsbError } from './fabric_errors';
import { FabricLink, clearEndpointHalts, type FabricLinkEvent, type ListenMode } from './fabric_link';
import {
  formatFabricLegLabel,
  formatFabricPortDisplay,
  resolveFabricLegFromDevice,
  sortFabricDevicesBySerial,
} from './fabric_port';
import {
  INTERFACE_NUMBER,
  PRODUCT_ID,
  VENDOR_ID,
  type ParsedHeader,
} from './fabric_protocol';
import type { FabricSessionMessage } from './fabric_session';
import type { FabricTransport } from './fabric_transport';
import { webUsbBlockedReason } from './webusb_env';

export { FabricUsbError } from './fabric_errors';

const SELECTED_SERIAL_KEY = 'rocketbox_usb_serial';
const USB_OPEN_ATTEMPTS = 3;

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

function legacySerialKey(leg: number): string {
  return `rocketbox_serial_port${leg}`;
}

function getSavedSerial(): string | null {
  const primary = sessionStorage.getItem(SELECTED_SERIAL_KEY);
  if (primary) {
    return primary;
  }
  for (let leg = 0; leg < 4; leg += 1) {
    const legacy = sessionStorage.getItem(legacySerialKey(leg));
    if (legacy) {
      sessionStorage.setItem(SELECTED_SERIAL_KEY, legacy);
      return legacy;
    }
  }
  return null;
}

function clearSavedSerial(): void {
  sessionStorage.removeItem(SELECTED_SERIAL_KEY);
  for (let leg = 0; leg < 4; leg += 1) {
    sessionStorage.removeItem(legacySerialKey(leg));
  }
}

function isFabricDevice(device: USBDevice): boolean {
  return device.vendorId === VENDOR_ID && device.productId === PRODUCT_ID;
}

async function resetDeviceHandle(device: USBDevice): Promise<void> {
  if (!device.opened) {
    return;
  }
  try {
    await device.releaseInterface(INTERFACE_NUMBER);
  } catch {
    /* best effort */
  }
  try {
    await device.close();
  } catch {
    /* best effort */
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
  // Match the native libusb path: clear any stale endpoint halt so the first
  // transferOut/transferIn after connect is not wedged by a halted endpoint.
  await clearEndpointHalts(device);
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
  throw lastErr instanceof FabricUsbError ? lastErr : mapUsbOpenError(lastErr);
}

function rememberSerial(device: USBDevice): void {
  if (device.serialNumber) {
    sessionStorage.setItem(SELECTED_SERIAL_KEY, device.serialNumber);
  }
}

function findPairedDevice(devices: USBDevice[]): USBDevice | null {
  const sorted = sortFabricDevicesBySerial(devices);
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
  private resolvedFabricLeg = 0;
  private readonly link: FabricLink;
  private unsubscribeLink: (() => void) | null = null;

  constructor() {
    this.link = new FabricLink(() => this.device, 0);
  }

  getFabricPortIndex(): number {
    return this.resolvedFabricLeg;
  }

  getFabricLeg(): number {
    return this.resolvedFabricLeg;
  }

  getSerialNumber(): string {
    return this.device?.serialNumber?.trim() ?? '';
  }

  private refreshResolvedLeg(): void {
    if (!this.device) {
      return;
    }
    this.resolvedFabricLeg = resolveFabricLegFromDevice(this.device);
    this.link.setFabricLeg(this.resolvedFabricLeg);
    boothLog(
      this.resolvedFabricLeg,
      'usb_connect',
      formatFabricLegLabel(this.resolvedFabricLeg, this.device.serialNumber),
    );
    // Surface the raw cable serial so two booth devices can be confirmed to be
    // on DIFFERENT legs — identical serials share a leg and never see peers.
    boothLog(this.resolvedFabricLeg, 'cable_serial', this.device.serialNumber || '(none)');
  }

  setListenMode(mode: ListenMode): void {
    this.link.setListenMode(mode);
  }

  ensureListening(): void {
    this.link.ensureListening();
  }

  subscribeSession(handler: (message: FabricSessionMessage) => void): () => void {
    if (this.unsubscribeLink) {
      this.unsubscribeLink();
    }
    this.unsubscribeLink = this.link.subscribe((event: FabricLinkEvent) => {
      if (event.type === 'session') {
        handler(event.message);
      }
    });
    return () => {
      this.unsubscribeLink?.();
      this.unsubscribeLink = null;
    };
  }

  async waitForIdle(): Promise<void> {
    await this.link.waitForIdle();
  }

  async prepareForPayloadSend(): Promise<void> {
    await this.link.prepareForPayloadSend();
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

  static clearSavedUsbPairing(): void {
    clearSavedSerial();
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
    this.refreshResolvedLeg();
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
      clearSavedSerial();
      throw new FabricUsbError(
        'Previously paired device not found — tap Connect USB and pick your cable in the browser dialog',
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
    this.refreshResolvedLeg();
    return this.describeDevice();
  }

  describeDevice(): string {
    if (!this.device) {
      return '';
    }
    return formatFabricPortDisplay(this.resolvedFabricLeg, this.device.serialNumber);
  }

  async disconnect(): Promise<void> {
    this.link.setListenMode('off');
    this.link.stopListenLoop();
    if (!this.device?.opened) {
      this.device = null;
      return;
    }
    try {
      await this.device.releaseInterface(INTERFACE_NUMBER);
    } catch {
      /* best effort */
    }
    try {
      await this.device.close();
    } finally {
      this.device = null;
    }
  }

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
        /* best effort */
      }
    }
    clearSavedSerial();
  }

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
    this.link.setListenMode('off');
    this.device = null;
  }

  async sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    await this.link.sendPayload(payload, onProgress, filename);
  }

  async receiveHeader(): Promise<ParsedHeader> {
    const message = await this.link.tryReceiveSessionMessage(2000);
    if (message) {
      throw new FabricUsbError('Expected payload header, got session message');
    }
    throw new FabricUsbError('receiveHeader not used — use receiveFileTransfer');
  }

  async receivePayload(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    const { data } = await this.link.receiveFileTransfer(15000, fileSize, onProgress);
    return data;
  }

  async discardPayload(fileSize: number): Promise<void> {
    await this.receivePayload(fileSize);
  }

  async receiveBytes(onProgress?: (done: number, total: number) => void): Promise<Uint8Array> {
    const { data } = await this.link.receiveFileTransfer(15000, 0, onProgress);
    return data;
  }

  async sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    await this.link.sendSessionMessage(message);
  }

  async tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    return this.link.tryReceiveSessionMessage(headerTimeoutMs);
  }

  async receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    return this.link.receiveFileTransfer(headerTimeoutMs, expectedBytes, onProgress);
  }
}

export async function readFilePayload(file: File): Promise<Uint8Array> {
  const buffer = await file.arrayBuffer();
  return new Uint8Array(buffer);
}