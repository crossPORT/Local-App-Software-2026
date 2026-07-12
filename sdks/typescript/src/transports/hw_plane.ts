import type { SystemInfo } from '../system_info';
import { fabricDebugLog } from '../fabric/debug_log';
import { FabricUsbError } from '../fabric/errors';
import {
  displayPortFromLeg,
  formatFabricPortDisplay,
  legFromWirePort,
  resolveFabricLegFromDevice,
} from '../fabric/port';
import { receiveStatusFromAnnounceNote } from '../fabric/announce_note';
import type { FabricSessionMessage } from '../fabric/session_types';
import type { ListenMode } from '../fabric/types';
import { DataListen } from './data_listen';
import { DataRecv } from './data_recv';
import { DataSend } from './data_send';
import { writeSwitch } from './usb_switch';
import { openAndClaim, pickDevice, releaseDevice, resetAndClaim } from './usb_open';
import type { UsbEndpoints } from './usb_ids';
import { rememberSerial } from './usb_pairing';
import { resolvePairedUsbDevice } from './usb_pair_ops';

/** HW USB: ROCKETBX transfer; switch clear on connect. */
export class HwPlane {
  private device: USBDevice | null = null;
  private eps: UsbEndpoints | null = null;
  private leg = 0;
  private systemId = '';
  private listen: DataListen;
  private send: DataSend;
  private recv: DataRecv;
  private readonly peers = new Map<string, SystemInfo>();
  private readonly sessionHandlers = new Set<(m: FabricSessionMessage) => void>();
  private unsubListen: (() => void) | null = null;

  constructor() {
    this.listen = new DataListen(() => this.device, () => this.eps, () => this.leg);
    this.send = new DataSend(() => this.device, () => this.eps, () => this.leg, this.listen);
    this.recv = new DataRecv(
      () => this.device,
      () => this.eps,
      () => this.leg,
      this.listen,
      (m) => this.dispatchSession(m),
    );
  }

  get connected(): boolean {
    return this.device?.opened === true && this.eps != null;
  }
  getSystemId(): string {
    return this.systemId;
  }
  getLeg(): number {
    return this.leg;
  }
  getSerial(): string {
    return this.device?.serialNumber?.trim() ?? '';
  }
  describe(): string {
    return formatFabricPortDisplay(this.leg, this.getSerial());
  }
  getDevice(): USBDevice | null {
    return this.device;
  }

  subscribeSession(h: (m: FabricSessionMessage) => void): () => void {
    this.sessionHandlers.add(h);
    return () => this.sessionHandlers.delete(h);
  }

  setListenMode(m: ListenMode): void {
    this.listen.setListenMode(m);
  }
  ensureListening(): void {
    this.listen.ensureListening();
  }
  prepareForPayloadSend(): Promise<void> {
    return this.send.prepareForPayloadSend();
  }
  waitForIdle(): Promise<void> {
    return this.send.waitForIdle();
  }

  /** Explicit port switch (C++ `switch_port_core` parity). */
  async switchPort(destDisplayPort: number): Promise<void> {
    if (!this.device || !this.eps) throw new FabricUsbError('USB not connected');
    await writeSwitch(this.device, this.eps.ep4Out, destDisplayPort);
  }

  /** dest=0 clears sticky p2p left by earlier experiments. */
  private async clearSwitch(): Promise<void> {
    if (!this.device || !this.eps) return;
    try {
      await this.switchPort(0);
      fabricDebugLog(this.leg, 'switch_clear', 'dest=0');
    } catch (err) {
      fabricDebugLog(this.leg, 'switch_clear_fail', (err as Error).message);
    }
  }

  async connect(existing?: USBDevice): Promise<string> {
    await this.disconnect();
    this.device = existing ?? (await pickDevice());
    this.eps = await openAndClaim(this.device);
    try {
      this.leg = resolveFabricLegFromDevice(this.device);
    } catch {
      this.leg = 0;
    }
    this.systemId = `sys-port-${displayPortFromLeg(this.leg)}`;
    rememberSerial(this.device);
    await this.clearSwitch();
    this.unsubListen = this.listen.subscribe((m) => this.dispatchSession(m));
    this.listen.setListenMode('always');
    fabricDebugLog(this.leg, 'usb_connect', this.describe());
    fabricDebugLog(this.leg, 'cable_serial', this.getSerial() || '(none)');
    return this.describe();
  }

  async reconnectKnown(): Promise<string> {
    const device = await resolvePairedUsbDevice();
    return this.connect(device);
  }

  async disconnect(): Promise<void> {
    this.listen.setListenMode('off');
    this.unsubListen?.();
    this.unsubListen = null;
    this.peers.clear();
    await releaseDevice(this.device);
    this.device = null;
    this.eps = null;
    this.systemId = '';
  }

  async resetConnection(): Promise<string> {
    if (!this.device) throw new FabricUsbError('USB not connected');
    this.listen.setListenMode('off');
    this.eps = await resetAndClaim(this.device);
    await this.clearSwitch();
    this.listen.setListenMode('always');
    return this.describe();
  }

  /** Validate peer id only — no port switch (C++ send_file_core parity). */
  async ensureCircuit(peerSystemId: string): Promise<void> {
    if (!this.device || !this.eps) throw new FabricUsbError('USB not connected');
    const m = peerSystemId.match(/^sys-port-(\d+)$/);
    const dest = m ? Number.parseInt(m[1]!, 10) : 0;
    if (dest < 1 || dest > 4) throw new FabricUsbError(`Invalid peer system id ${peerSystemId}`);
    fabricDebugLog(this.leg, 'circuit_ready', `peer=${peerSystemId} (no switch)`);
  }

  listSystems(): SystemInfo[] {
    return [...this.peers.values()];
  }

  async syncSystems(handler: (systems: SystemInfo[]) => void): Promise<void> {
    handler(this.listSystems());
  }

  sendSessionMessage = (msg: FabricSessionMessage): Promise<void> =>
    this.send.sendSessionMessage(msg);
  sendBytes = (
    p: Uint8Array,
    onProgress?: (d: number, t: number) => void,
    filename?: string,
  ): Promise<void> => this.send.sendPayload(p, onProgress, filename ?? '');
  receiveFileTransfer = (
    ms: number,
    expected?: number,
    onProgress?: (d: number, t: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> =>
    this.recv.receiveFileTransfer(ms, expected, onProgress);

  private dispatchSession(msg: FabricSessionMessage): void {
    if (msg.kind === 'announce' && msg.from_name) this.noteAnnounce(msg);
    for (const h of this.sessionHandlers) h(msg);
  }

  private noteAnnounce(msg: FabricSessionMessage): void {
    const portMatch = (msg.note ?? '').match(/(?:^|;)\s*port=(\d+)/);
    const wire = portMatch ? Number.parseInt(portMatch[1]!, 10) : NaN;
    const leg = legFromWirePort(wire);
    if (leg == null || leg === this.leg) return;
    const id = `sys-port-${leg + 1}`;
    const receive = receiveStatusFromAnnounceNote(msg.note);
    this.peers.set(id, {
      id,
      name: msg.from_name,
      status: receive === 'busy' ? 'busy' : 'reachable',
      receive,
    });
  }
}
