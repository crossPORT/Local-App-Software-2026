import { boothLog } from '../src/lib/booth_log';
import { FabricUsbError } from '../src/lib/fabric_errors';
import { FabricLink, type FabricLinkEvent, type ListenMode } from '../src/lib/fabric_link';
import { formatFabricLegLabel, resolveFabricLegFromDevice } from '../src/lib/fabric_port';
import { systemNameForLeg } from '../src/lib/system_names';
import type { FabricSessionMessage } from '../src/lib/fabric_session';
import type { FabricTransport } from '../src/lib/fabric_transport';
import type { ParsedHeader } from '../src/lib/fabric_protocol';
import { fabricHubReset, SIM_CABLE_SERIALS } from './fabric_hub';
import { SimUsbDevice } from './fabric_sim_device';
import {
  clearSavedSimSerial, getSavedSimSerial, pickSimSerial, rememberSimSerial,
} from './fabric_sim_serial';

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

  static clearSavedSerial(): void {
    clearSavedSimSerial();
  }

  static hasSavedSerial(): boolean {
    return getSavedSimSerial() != null;
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

  async connect(serial?: string, options?: { resetHub?: boolean; remember?: boolean }): Promise<string> {
    if (options?.resetHub !== false) {
      fabricHubReset();
    }
    const chosen = pickSimSerial(serial);
    this.device = new SimUsbDevice(chosen);
    if (options?.remember !== false) {
      rememberSimSerial(chosen);
    }
    this.refreshResolvedLeg();
    this.link.setListenMode('always');
    return this.describeDevice();
  }

  async reconnectKnown(): Promise<string> {
    const saved = getSavedSimSerial();
    if (!saved) {
      throw new FabricUsbError('No saved sim cable — click Connect');
    }
    this.device = new SimUsbDevice(saved);
    this.refreshResolvedLeg();
    this.link.setListenMode('always');
    return this.describeDevice();
  }

  describeDevice(): string {
    return this.device ? systemNameForLeg(this.resolvedFabricLeg) : '';
  }

  async disconnect(): Promise<void> {
    this.link.setListenMode('off');
    this.link.stopListenLoop();
    this.device = null;
  }

  async forgetThisDevice(): Promise<void> {
    await this.disconnect();
    clearSavedSimSerial();
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
  ) {
    return this.link.receiveFileTransfer(headerTimeoutMs, expectedBytes, onProgress);
  }
}
