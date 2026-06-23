import { boothLog } from '../src/lib/booth_log';
import { FabricUsbError } from '../src/lib/fabric_errors';
import { FabricLink, type FabricLinkEvent, type ListenMode } from '../src/lib/fabric_link';
import {
  formatFabricLegLabel,
  formatFabricPortDisplay,
  resolveFabricLegFromDevice,
} from '../src/lib/fabric_port';
import type { FabricSessionMessage } from '../src/lib/fabric_session';
import type { FabricTransport } from '../src/lib/fabric_transport';
import type { ParsedHeader } from '../src/lib/fabric_protocol';
import { fabricHubReset, SIM_CABLE_SERIALS } from './fabric_hub';
import { SimUsbDevice } from './fabric_sim_device';

const SELECTED_SERIAL_KEY = 'rocketbox_sim_serial';

function getSavedSerial(): string | null {
  return sessionStorage.getItem(SELECTED_SERIAL_KEY);
}

function rememberSerial(serial: string): void {
  sessionStorage.setItem(SELECTED_SERIAL_KEY, serial);
}

function clearSavedSerial(): void {
  sessionStorage.removeItem(SELECTED_SERIAL_KEY);
}

async function pickSimSerial(): Promise<string> {
  const saved = getSavedSerial();
  if (saved && SIM_CABLE_SERIALS.includes(saved as (typeof SIM_CABLE_SERIALS)[number])) {
    return saved;
  }
  if (typeof window === 'undefined') {
    return SIM_CABLE_SERIALS[0]!;
  }
  const lines = SIM_CABLE_SERIALS.map((serial, index) => `${index + 1}. ${serial} (port ${index + 1})`).join('\n');
  const raw = window.prompt(
    `Pick a simulated cable (use a different serial in each tab):\n${lines}`,
    '1',
  );
  const choice = Number.parseInt(raw ?? '1', 10);
  const index = Number.isFinite(choice) ? Math.max(0, Math.min(SIM_CABLE_SERIALS.length - 1, choice - 1)) : 0;
  return SIM_CABLE_SERIALS[index]!;
}

export class FabricSimSession implements FabricTransport {
  private device: SimUsbDevice | null = null;
  private resolvedFabricLeg = 0;
  private readonly link: FabricLink;
  private unsubscribeLink: (() => void) | null = null;

  constructor() {
    this.link = new FabricLink(() => this.device as unknown as USBDevice, 0);
  }

  getFabricPortIndex(): number {
    return this.resolvedFabricLeg;
  }

  getFabricLeg(): number {
    return this.resolvedFabricLeg;
  }

  getSerialNumber(): string {
    return this.device?.serialNumber ?? '';
  }

  get connected(): boolean {
    return this.device != null && this.device.opened;
  }

  static async countFabricDevices(): Promise<number> {
    return SIM_CABLE_SERIALS.length;
  }

  static hasSavedSerial(): boolean {
    return getSavedSerial() != null;
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

  async connect(): Promise<string> {
    fabricHubReset();
    const serial = await pickSimSerial();
    this.device = new SimUsbDevice(serial);
    rememberSerial(serial);
    this.refreshResolvedLeg();
    this.link.setListenMode('always');
    return this.describeDevice();
  }

  async reconnectKnown(): Promise<string> {
    const saved = getSavedSerial();
    if (!saved) {
      throw new FabricUsbError('No saved sim cable — click Connect');
    }
    this.device = new SimUsbDevice(saved);
    this.refreshResolvedLeg();
    this.link.setListenMode('always');
    return this.describeDevice();
  }

  describeDevice(): string {
    if (!this.device) {
      return '';
    }
    return `Sim · ${formatFabricPortDisplay(this.resolvedFabricLeg)}`;
  }

  async disconnect(): Promise<void> {
    this.link.setListenMode('off');
    this.link.stopListenLoop();
    this.device = null;
  }

  async forgetThisDevice(): Promise<void> {
    await this.disconnect();
    clearSavedSerial();
    fabricHubReset();
  }

  async resetConnection(): Promise<string> {
    if (!this.device) {
      throw new FabricUsbError('Sim not connected');
    }
    fabricHubReset();
    return this.describeDevice();
  }

  ownsDevice(_usbDevice: USBDevice): boolean {
    return false;
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
