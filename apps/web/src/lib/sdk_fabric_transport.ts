import type { Session, SystemInfo, Transport } from '@rocketbox/sdk';
import { fabricSimEnabled } from '../../sim/fabric_sim';
import type { FabricSessionMessage } from './fabric_session';
import type { FabricTransport, ListenMode } from './fabric_transport';
import { CircuitDataBuffer } from './sdk_fabric_buffer';
import {
  clearSavedSerial,
  countFabricDevices,
  hasSavedSerial,
} from './sdk_fabric_attach';
import { planeIo } from './sdk_fabric_plane_io';
import { attachOrReplace, teardownPlane } from './sdk_fabric_life';
import type { PlaneState } from './sdk_fabric_plane';
import type { ConnHooks } from './sdk_fabric_wire';

/** FabricTransport backed by @rocketbox/sdk (4-EP Session/Connection). */
export class SdkFabricTransport implements FabricTransport {
  private hub: Session | null = null;
  private transport: Transport | null = null;
  private usbDevice: USBDevice | null = null;
  private port = 1;
  private unsubIncoming: (() => void) | null = null;
  private unsubDetached: (() => void) | null = null;
  private connectHandlers = new Set<() => void>();
  private readonly hooks: ConnHooks = { unsubMsg: null, unsubData: null, unsubClosed: null };
  private readonly plane: PlaneState = {
    session: null,
    connection: null,
    dataBuf: new CircuitDataBuffer(),
    sessionHandlers: new Set(),
    hooks: this.hooks,
  };
  private readonly io = planeIo(this.plane);

  get connected(): boolean {
    return this.hub != null && (this.transport?.connected ?? false);
  }
  getSystemId(): string {
    return this.hub?.systemId ?? '';
  }
  getFabricPortIndex(): number {
    return this.port - 1;
  }
  getFabricLeg(): number {
    return this.port - 1;
  }
  getSerialNumber(): string {
    return this.usbDevice?.serialNumber ?? `sdk-port-${this.port}`;
  }

  describeDevice(): string {
    if (fabricSimEnabled()) return `Sim fabric port ${this.port} (${this.getSystemId()})`;
    return `USB ${this.getSerialNumber()} port ${this.port} (${this.getSystemId()})`;
  }

  async connect(): Promise<string> {
    await this.attachFresh();
    return this.describeDevice();
  }

  async reconnectKnown(): Promise<string> {
    await this.attachFresh();
    return this.describeDevice();
  }

  async disconnect(): Promise<void> {
    await this.teardown();
  }

  async forgetThisDevice(): Promise<void> {
    clearSavedSerial();
    await this.teardown();
  }

  async resetConnection(): Promise<string> {
    try {
      await this.plane.connection?.close();
    } catch {
      /* best effort */
    }
    this.plane.connection = null;
    this.plane.dataBuf.clear();
    return this.describeDevice();
  }

  ownsDevice(d: USBDevice): boolean {
    return this.usbDevice === d;
  }

  markDisconnected(): void {
    void this.teardown();
  }

  setListenMode(_m: ListenMode): void {}
  ensureListening(): void {}
  prepareForPayloadSend(): Promise<void> {
    return Promise.resolve();
  }
  waitForIdle(): Promise<void> {
    return Promise.resolve();
  }

  subscribeSession(handler: (m: FabricSessionMessage) => void): () => void {
    this.plane.sessionHandlers.add(handler);
    return () => this.plane.sessionHandlers.delete(handler);
  }

  subscribeConnect(handler: () => void): () => void {
    this.connectHandlers.add(handler);
    return () => this.connectHandlers.delete(handler);
  }

  async syncSystems(handler: (systems: SystemInfo[]) => void): Promise<void> {
    if (!this.hub) return;
    handler(await this.hub.listSystems());
  }

  ensureCircuit = this.io.ensureCircuit;
  sendSessionMessage = this.io.sendSessionMessage;
  sendBytes = this.io.sendBytes;
  receiveFileTransfer = this.io.receiveFileTransfer;
  receiveHeader = this.io.receiveHeader;
  receivePayload = this.io.receivePayload;
  discardPayload = this.io.discardPayload;
  receiveBytes = this.io.receiveBytes;
  tryReceiveSessionMessage = this.io.tryReceiveSessionMessage;

  static countFabricDevices = countFabricDevices;
  static hasSavedSerial = hasSavedSerial;
  static clearSavedUsbPairing = clearSavedSerial;

  private async attachFresh(): Promise<void> {
    const next = await attachOrReplace({
      plane: this.plane,
      hooks: this.hooks,
      transport: this.transport,
      unsubIncoming: this.unsubIncoming,
      unsubDetached: this.unsubDetached,
      connectHandlers: this.connectHandlers,
      onLost: () => {
        void this.teardown();
      },
    });
    this.hub = next.hub;
    this.transport = next.transport;
    this.usbDevice = next.device;
    this.port = next.port;
    this.unsubIncoming = next.unsubIncoming;
    this.unsubDetached = next.unsubDetached;
  }

  private async teardown(): Promise<void> {
    await teardownPlane({
      plane: this.plane,
      hooks: this.hooks,
      transport: this.transport,
      unsubIncoming: this.unsubIncoming,
      unsubDetached: this.unsubDetached,
    });
    this.unsubIncoming = null;
    this.unsubDetached = null;
    this.transport = null;
    this.hub = null;
    this.usbDevice = null;
  }
}
