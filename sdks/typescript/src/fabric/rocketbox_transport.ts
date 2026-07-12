import type { Session } from '../session_core';
import type { SystemInfo } from '../system_info';
import type { Transport } from '../transport';
import {
  clearSavedUsbPairing,
  countFabricDevices,
  hasSavedSerial,
} from '../transports/usb_pairing';
import { forgetUsbDevice } from '../transports/usb_pair_ops';
import { HwPlane } from '../transports/hw_plane';
import { countSimFabricDevices } from './rocketbox_attach';
import type { ConnHooks } from './rocketbox_wire';
import type { FabricSessionMessage } from './session_types';
import type { FabricTransport, ListenMode } from './types';
import { formatFabricPortDisplay } from './port';
import { describeSimPort, emptyPlane } from './rocketbox_sim_life';
import { transportIo } from './rocketbox_transport_io';
import { hostSimConnect, hostSimTeardown, type SimHostState } from './rocketbox_transport_sim';
/** Sole FabricTransport. HW: switch + ROCKETBX transfer. Sim: Session. */
export class RocketBoxTransport implements FabricTransport {
  private hw: HwPlane | null = null;
  private hub: Session | null = null;
  private transport: Transport | null = null;
  private port: number;
  private readonly simulate: boolean;
  private unsubIncoming: (() => void) | null = null;
  private unsubDetached: (() => void) | null = null;
  private unsubHwSession: (() => void) | null = null;
  private connectHandlers = new Set<() => void>();
  private readonly hooks: ConnHooks = { unsubMsg: null, unsubData: null, unsubClosed: null };
  private readonly plane = emptyPlane(this.hooks);
  constructor(port = 1, simulate = false) {
    this.port = port;
    this.simulate = simulate;
  }
  private io() {
    return transportIo({ simulate: this.simulate, hw: this.hw, plane: this.plane });
  }
  private simState(): SimHostState {
    return {
      plane: this.plane,
      hooks: this.hooks,
      transport: this.transport,
      unsubIncoming: this.unsubIncoming,
      unsubDetached: this.unsubDetached,
      connectHandlers: this.connectHandlers,
      port: this.port,
      hub: this.hub,
      describe: () => this.describeDevice(),
      onLost: () => {
        void this.disconnect();
      },
    };
  }
  private applySim(s: SimHostState): void {
    this.hub = s.hub;
    this.transport = s.transport;
    this.port = s.port;
    this.unsubIncoming = s.unsubIncoming;
    this.unsubDetached = s.unsubDetached;
  }
  get connected(): boolean {
    return this.simulate
      ? this.hub != null && (this.transport?.connected ?? false)
      : (this.hw?.connected ?? false);
  }
  getSystemId(): string {
    return this.simulate ? (this.hub?.systemId ?? '') : (this.hw?.getSystemId() ?? '');
  }
  getFabricPortIndex(): number {
    if (!this.simulate && this.hw) return this.hw.getLeg();
    const m = this.getSystemId().match(/^sys-port-(\d+)$/);
    return m ? Number.parseInt(m[1]!, 10) - 1 : this.port - 1;
  }
  getFabricLeg = (): number => this.getFabricPortIndex();
  getSerialNumber(): string {
    return this.simulate ? `sdk-port-${this.port}` : (this.hw?.getSerial() ?? '');
  }
  describeDevice(): string {
    if (this.simulate && !this.hub) return describeSimPort(this.port);
    return formatFabricPortDisplay(this.getFabricPortIndex(), this.getSerialNumber());
  }
  async connect(): Promise<string> {
    if (this.simulate) {
      const s = this.simState();
      const desc = await hostSimConnect(s);
      this.applySim(s);
      return desc;
    }
    this.hw ??= new HwPlane();
    const desc = await this.hw.connect();
    this.bindHwSession();
    this.connectHandlers.forEach((h) => h());
    return desc;
  }
  async reconnectKnown(): Promise<string> {
    if (this.simulate) return this.connect();
    this.hw ??= new HwPlane();
    const desc = await this.hw.reconnectKnown();
    this.bindHwSession();
    this.connectHandlers.forEach((h) => h());
    return desc;
  }
  async disconnect(): Promise<void> {
    if (this.simulate) {
      const s = this.simState();
      await hostSimTeardown(s);
      this.applySim(s);
      return;
    }
    this.unsubHwSession?.();
    this.unsubHwSession = null;
    await this.hw?.disconnect();
  }
  async forgetThisDevice(): Promise<void> {
    const device = this.hw?.getDevice() ?? null;
    await this.disconnect();
    if (!this.simulate) await forgetUsbDevice(device);
  }
  async resetConnection(): Promise<string> {
    if (!this.simulate) return this.hw?.resetConnection() ?? this.describeDevice();
    try {
      await this.plane.connection?.close();
    } catch {
      /* best effort */
    }
    this.plane.connection = null;
    this.plane.dataBuf.clear();
    if (this.transport?.recover) await this.transport.recover();
    return this.describeDevice();
  }
  ownsDevice = (d: USBDevice): boolean => this.hw?.getDevice() === d;
  markDisconnected = (): void => {
    void this.disconnect();
  };
  setListenMode(m: ListenMode): void {
    if (!this.simulate) this.hw?.setListenMode(m);
  }
  ensureListening(): void {
    if (!this.simulate) this.hw?.ensureListening();
  }
  prepareForPayloadSend = (): Promise<void> =>
    this.simulate ? Promise.resolve() : (this.hw?.prepareForPayloadSend() ?? Promise.resolve());
  waitForIdle = (): Promise<void> =>
    this.simulate ? Promise.resolve() : (this.hw?.waitForIdle() ?? Promise.resolve());
  subscribeSession(handler: (m: FabricSessionMessage) => void): () => void {
    this.plane.sessionHandlers.add(handler);
    return () => this.plane.sessionHandlers.delete(handler);
  }
  subscribeConnect(handler: () => void): () => void {
    this.connectHandlers.add(handler);
    return () => this.connectHandlers.delete(handler);
  }
  async listSystems(): Promise<SystemInfo[]> {
    if (this.simulate) return this.hub ? this.hub.listSystems() : [];
    return this.hw?.listSystems() ?? [];
  }
  async syncSystems(handler: (systems: SystemInfo[]) => void): Promise<void> {
    handler(await this.listSystems());
  }
  ensureCircuit = (id: string) => this.io().ensureCircuit(id);
  sendSessionMessage = (m: FabricSessionMessage) => this.io().sendSessionMessage(m);
  sendBytes = (
    p: Uint8Array,
    onProgress?: (d: number, t: number) => void,
    filename?: string,
  ) => this.io().sendBytes(p, onProgress, filename);
  receiveFileTransfer = (
    ms: number,
    expected?: number,
    onProgress?: (d: number, t: number) => void,
  ) => this.io().receiveFileTransfer(ms, expected, onProgress);
  receiveHeader = () => this.io().receiveHeader();
  receivePayload = (n: number, onProgress?: (d: number, t: number) => void) =>
    this.io().receivePayload(n, onProgress);
  discardPayload = (n: number) => this.io().discardPayload(n);
  receiveBytes = (onProgress?: (d: number, t: number) => void) => this.io().receiveBytes(onProgress);
  tryReceiveSessionMessage = (ms: number) => this.io().tryReceiveSessionMessage(ms);
  static countFabricDevices = countFabricDevices;
  static hasSavedSerial = hasSavedSerial;
  static clearSavedUsbPairing = clearSavedUsbPairing;
  static countSimFabricDevices = countSimFabricDevices;
  private bindHwSession(): void {
    this.unsubHwSession?.();
    this.unsubHwSession = this.hw!.subscribeSession((m) => {
      this.plane.sessionHandlers.forEach((h) => h(m));
    });
  }
}
