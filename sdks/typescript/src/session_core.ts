import type { Transport } from './transport';
import type { SystemInfo } from './system_info';
import {
  MSG_LIST,
  MSG_CONNECT,
  MSG_DISCONNECT,
  MSG_SYSTEMS,
  MSG_CIRCUIT_UP,
  MSG_CIRCUIT_DOWN,
} from './messages';
import { Connection } from './connection';

export { Connection, type ConnectionState } from './connection';
export { RocketBox } from './rocketbox';

type SystemsChangedCallback = (systems: SystemInfo[]) => void;
type IncomingCircuitCallback = (connection: Connection) => void;

export class Session {
  private activeConnection: Connection | null = null;
  private attachedPortId: number = -1;
  private nextTxn = 1;
  
  public systemId: string = '';
  
  // Event listeners
  private systemsChangedListeners = new Set<SystemsChangedCallback>();
  private incomingCircuitListeners = new Set<IncomingCircuitCallback>();
  private detachedListeners = new Set<() => void>();

  constructor(private readonly transport: Transport, private readonly port: number) {}

  async init(): Promise<void> {
    // Register handlers before opening so early EP traffic is not dropped.
    this.transport.onEP3Received((header, payload) => {
      this.handleControlMessage(header, payload);
    });
    this.transport.onEP2Received((data) => {
      if (this.activeConnection) {
        this.activeConnection.handleData(data);
      }
    });
    this.transport.onDisconnected(() => {
      this.activeConnection?.handleRemoteClose('detached');
      this.activeConnection = null;
      this.detachedListeners.forEach((l) => l());
    });

    await this.transport.init();
    // Port claimed via WS ?port=N (or TCP 0xC1 claim). No ATTACH — matches HW/C++.
    this.attachedPortId = this.port - 1;
    this.systemId = `sys-port-${this.port}`;
  }

  private handleControlMessage(header: DataView, payload: Uint8Array): void {
    const type = header.getUint8(1);

    switch (type) {
      case MSG_SYSTEMS: {
        const systems = this.parseSystemsPayload(payload);
        this.systemsChangedListeners.forEach(l => l(systems));
        break;
      }
      case MSG_CIRCUIT_UP: {
        const peerSysId = new TextDecoder().decode(payload);
        const conn = new Connection(peerSysId, this);
        this.activeConnection = conn;
        this.incomingCircuitListeners.forEach(l => l(conn));
        break;
      }
      case MSG_CIRCUIT_DOWN: {
        this.activeConnection?.handleRemoteClose('closed');
        this.activeConnection = null;
        break;
      }
    }
  }

  private parseSystemsPayload(payload: Uint8Array): SystemInfo[] {
    if (payload.length < 4) return [];
    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    const count = view.getUint32(0);
    const systems: SystemInfo[] = [];
    let offset = 4;
    for (let i = 0; i < count; i++) {
      if (offset >= payload.length) break;
      const idLen = view.getUint8(offset++);
      const id = new TextDecoder().decode(payload.subarray(offset, offset + idLen));
      offset += idLen;

      const nameLen = view.getUint8(offset++);
      const name = new TextDecoder().decode(payload.subarray(offset, offset + nameLen));
      offset += nameLen;

      const statusVal = view.getUint8(offset++);
      const status = statusVal === 1 ? 'reachable' : statusVal === 2 ? 'busy' : 'offline';

      // Do not include ourselves as a remote peer
      if (id !== this.systemId) {
        systems.push({ id, name, status });
      }
    }
    return systems;
  }

  public async listSystems(): Promise<SystemInfo[]> {
    const header = this.buildControlHeader(MSG_LIST, this.getTxn(), 0, 0);
    const [_, payload] = await this.transport.writeEP4(header);
    return this.parseSystemsPayload(payload);
  }

  public async connect(targetSystemId: string): Promise<Connection> {
    if (this.activeConnection) {
      throw new Error('AlreadyConnected');
    }
    const payload = new TextEncoder().encode(targetSystemId);
    const header = this.buildControlHeader(MSG_CONNECT, this.getTxn(), 0, payload.length);
    const fullPacket = new Uint8Array(header.length + payload.length);
    fullPacket.set(header, 0);
    fullPacket.set(payload, header.length);

    await this.transport.writeEP4(fullPacket);
    
    const conn = new Connection(targetSystemId, this);
    this.activeConnection = conn;
    return conn;
  }

  public async disconnectConnection(): Promise<void> {
    if (!this.activeConnection) return;
    const header = this.buildControlHeader(MSG_DISCONNECT, this.getTxn(), 0, 0);
    await this.transport.writeEP4(header);
    this.activeConnection = null;
  }

  public writeEP1(data: Uint8Array): void {
    this.transport.writeEP1(data);
  }

  private buildControlHeader(type: number, txn: number, arg: number, payloadLen: number): Uint8Array {
    const packet = new Uint8Array(1 + 12);
    packet[0] = 0x04; // EP4 Control OUT
    const view = new DataView(packet.buffer, packet.byteOffset + 1, 12);
    view.setUint8(0, 0x01); // ver
    view.setUint8(1, type);
    view.setUint16(2, txn);
    view.setUint32(4, arg);
    view.setUint32(8, payloadLen);
    return packet;
  }

  private getTxn(): number {
    this.nextTxn = (this.nextTxn + 1) % 65535;
    return this.nextTxn === 0 ? 1 : this.nextTxn;
  }

  public onSystemsChanged(callback: SystemsChangedCallback): () => void {
    this.systemsChangedListeners.add(callback);
    return () => this.systemsChangedListeners.delete(callback);
  }

  public onIncomingCircuit(callback: IncomingCircuitCallback): () => void {
    this.incomingCircuitListeners.add(callback);
    return () => this.incomingCircuitListeners.delete(callback);
  }

  public onDetached(callback: () => void): () => void {
    this.detachedListeners.add(callback);
    return () => this.detachedListeners.delete(callback);
  }

  public closeConnectionLocal(): void {
    this.activeConnection = null;
  }
}

