import type { SystemInfo } from '../system_info';
import { debugLog } from '../debug_log';
import { RocketBoxError } from '../errors';
import { toDisplayPort, formatPortLabel, resolvePortIndexFromDevice } from '../port';
import type { SessionMessage } from '../session_types';
import type { AnnouncePresenceMode, ListenMode } from '../types';
import { sendAnnouncePresence } from './announce_presence';
import { noteAnnouncePeer } from './hw_peers';
import { DataListen } from './data_listen';
import { DataRecv } from './data_recv';
import { DataSend } from './data_send';
import { openAndClaim, pickDevice, releaseDevice, resetAndClaim } from './usb_open';
import type { UsbEndpoints } from './usb_ids';
import { rememberSerial } from './usb_pairing';
import { resolvePairedUsbDevice } from './usb_pair_ops';
import { SwitchDestCache } from './switch_cache';

/** HW USB: ROCKETBX transfer + EP4 switch for presence/circuit. */
export class HwPlane {
  private device: USBDevice | null = null;
  private eps: UsbEndpoints | null = null;
  private leg = 0;
  private systemId = '';
  private listen: DataListen;
  private send: DataSend;
  private recv: DataRecv;
  private readonly peers = new Map<string, SystemInfo>();
  private readonly sessionHandlers = new Set<(m: SessionMessage) => void>();
  private unsubListen: (() => void) | null = null;
  private readonly switches = new SwitchDestCache();
  private readonly rotateIndex = { current: 0 };

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
    return formatPortLabel(this.leg, this.getSerial());
  }
  getDevice(): USBDevice | null {
    return this.device;
  }

  subscribeSession(h: (m: SessionMessage) => void): () => void {
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

  async switchPort(destDisplayPort: number): Promise<void> {
    if (!this.device || !this.eps) throw new RocketBoxError('USB not connected', 'usb');
    await this.switches.switchIfNeeded(this.device, this.eps, destDisplayPort);
  }

  /** Live EP4 dest 1–4, or 0 if cleared. */
  switchDest(): number {
    const d = this.switches.last;
    return d != null && d >= 1 && d <= 4 ? d : 0;
  }

  async connect(existing?: USBDevice): Promise<string> {
    await this.disconnect();
    this.device = existing ?? (await pickDevice());
    this.eps = await openAndClaim(this.device);
    try {
      this.leg = resolvePortIndexFromDevice(this.device);
    } catch {
      this.leg = 0;
    }
    this.systemId = `sys-port-${toDisplayPort(this.leg)}`;
    this.switches.clear();
    this.rotateIndex.current = 0;
    rememberSerial(this.device);
    this.unsubListen = this.listen.subscribe((m) => this.dispatchSession(m));
    this.listen.setListenMode('always');
    debugLog(this.leg, 'usb_connect', this.describe());
    debugLog(this.leg, 'cable_serial', this.getSerial() || '(none)');
    return this.describe();
  }

  async reconnectKnown(): Promise<string> {
    return this.connect(await resolvePairedUsbDevice());
  }

  async disconnect(): Promise<void> {
    this.listen.setListenMode('off');
    this.unsubListen?.();
    this.unsubListen = null;
    this.peers.clear();
    this.switches.clear();
    await releaseDevice(this.device);
    this.device = null;
    this.eps = null;
    this.systemId = '';
  }

  async resetConnection(): Promise<string> {
    if (!this.device) throw new RocketBoxError('USB not connected', 'usb');
    this.listen.setListenMode('off');
    this.eps = await resetAndClaim(this.device);
    this.switches.clear();
    this.listen.setListenMode('always');
    return this.describe();
  }

  async ensureCircuit(peerSystemId: string): Promise<void> {
    if (!this.device || !this.eps) throw new RocketBoxError('USB not connected', 'usb');
    const m = peerSystemId.match(/^sys-port-(\d+)$/);
    const dest = m ? Number.parseInt(m[1]!, 10) : 0;
    if (dest < 1 || dest > 4) {
      throw new RocketBoxError(`Invalid peer system id ${peerSystemId}`, 'protocol');
    }
    await this.switches.switchIfNeeded(this.device, this.eps, dest);
    this.switches.markPreserve();
    debugLog(this.leg, 'circuit_ready', `peer=${peerSystemId} dest=${dest}`);
  }

  async sendAnnouncePresence(message: SessionMessage, mode: AnnouncePresenceMode): Promise<void> {
    if (!this.device || !this.eps) throw new RocketBoxError('USB not connected', 'usb');
    await sendAnnouncePresence(
      {
        device: this.device,
        eps: this.eps,
        portIndex: this.leg,
        switches: this.switches,
        rotateIndex: this.rotateIndex,
        sendSession: (m) => this.send.sendSessionMessage(m),
      },
      message,
      mode,
    );
  }

  listSystems(): SystemInfo[] {
    return [...this.peers.values()];
  }
  async syncSystems(handler: (systems: SystemInfo[]) => void): Promise<void> {
    handler(this.listSystems());
  }
  sendSessionMessage = (msg: SessionMessage): Promise<void> => this.send.sendSessionMessage(msg);
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

  private dispatchSession(msg: SessionMessage): void {
    noteAnnouncePeer(msg, this.leg, this.peers);
    for (const h of this.sessionHandlers) h(msg);
  }
}
