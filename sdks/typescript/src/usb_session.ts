import { boothLog } from './debug_log';
import { FabricLink, type FabricLinkEvent, type ListenMode } from './fabric_link';
import {
  formatFabricLegLabel,
  formatFabricPortDisplay,
  resolveFabricLegFromDevice,
} from './port';
import { type ParsedHeader } from './protocol';
import type { FabricSessionMessage } from './session_types';
import type { RocketBoxTransport } from './transport';
import { hasSavedSerial, isFabricDevice, clearSavedSerial } from './usb_pairing';
import { usbConnect, usbDisconnect, usbForgetThisDevice, usbReconnectKnown, usbResetConnection } from './usb_session_life';
import {
  sessionReceiveBytes,
  sessionReceiveHeader,
  sessionReceivePayload,
  sessionSendBytes,
} from './usb_session_io';

export { FabricUsbError } from './errors';
export { hasSavedSerial, clearSavedSerial as clearSavedUsbPairing } from './usb_pairing';

export class FabricUsbSession implements RocketBoxTransport {
  device: USBDevice | null = null;
  private resolvedFabricLeg = 0;
  readonly link: FabricLink;
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

  refreshResolvedLeg(): void {
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
    return hasSavedSerial();
  }

  static clearSavedUsbPairing(): void {
    clearSavedSerial();
  }

  connect(): Promise<string> {
    return usbConnect(this);
  }

  reconnectKnown(): Promise<string> {
    return usbReconnectKnown(this);
  }

  disconnect(): Promise<void> {
    return usbDisconnect(this);
  }

  forgetThisDevice(): Promise<void> {
    return usbForgetThisDevice(this);
  }

  resetConnection(): Promise<string> {
    return usbResetConnection(this);
  }

  describeDevice(): string {
    if (!this.device) {
      return '';
    }
    return formatFabricPortDisplay(this.resolvedFabricLeg, this.device.serialNumber);
  }

  ownsDevice(usbDevice: USBDevice): boolean {
    return this.device === usbDevice;
  }

  markDisconnected(): void {
    this.link.setListenMode('off');
    this.device = null;
  }

  sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    return sessionSendBytes(this, payload, onProgress, filename);
  }

  receiveHeader(): Promise<ParsedHeader> {
    return sessionReceiveHeader(this);
  }

  receivePayload(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    return sessionReceivePayload(this, fileSize, onProgress);
  }

  discardPayload(fileSize: number): Promise<void> {
    return this.receivePayload(fileSize).then(() => undefined);
  }

  receiveBytes(onProgress?: (done: number, total: number) => void): Promise<Uint8Array> {
    return sessionReceiveBytes(this, onProgress);
  }

  sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    return this.link.sendSessionMessage(message);
  }

  tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    return this.link.tryReceiveSessionMessage(headerTimeoutMs);
  }

  receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    return this.link.receiveFileTransfer(headerTimeoutMs, expectedBytes, onProgress);
  }
}
