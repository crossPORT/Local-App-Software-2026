import type { Transport } from './transport';

// Message Types
const MSG_ATTACH = 0x01;
const MSG_LIST = 0x02;
const MSG_CONNECT = 0x03;
const MSG_DISCONNECT = 0x04;
const MSG_KEEPALIVE = 0x05;

const MSG_ATTACHED = 0x81;
const MSG_SYSTEMS = 0x82;
const MSG_ACK = 0x83;
const MSG_NAK = 0x84;
const MSG_CIRCUIT_UP = 0x85;
const MSG_CIRCUIT_DOWN = 0x86;

export interface SystemInfo {
  id: string;
  name: string;
  status: 'reachable' | 'busy' | 'offline';
}

export type ConnectionState = 'open' | 'closing' | 'closed';

type SystemsChangedCallback = (systems: SystemInfo[]) => void;
type IncomingCircuitCallback = (connection: Connection) => void;
type ReceivedCallback = (bytes: Uint8Array) => void;
type MessageReceivedCallback = (message: Uint8Array) => void;
type ClosedCallback = (reason: string) => void;

export class RocketBox {
  static async attach(transport: Transport, port: number = 1): Promise<Session> {
    const session = new Session(transport, port);
    await session.init();
    return session;
  }
}

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
    await this.transport.init();

    this.transport.onEP3Received((header, payload) => {
      this.handleControlMessage(header, payload);
    });

    this.transport.onEP2Received((data) => {
      if (this.activeConnection) {
        this.activeConnection.handleData(data);
      }
    });

    // Send ATTACH control packet to register port
    await this.sendAttach();
  }

  private handleControlMessage(header: DataView, payload: Uint8Array): void {
    const type = header.getUint8(1);
    const arg = header.getUint32(4);

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

  private async sendAttach(): Promise<void> {
    const header = this.buildControlHeader(MSG_ATTACH, 0, this.port - 1, 0);
    const [reply] = await this.transport.writeEP4(header);
    this.attachedPortId = reply.getUint32(4);
    this.systemId = `sys-port-${this.attachedPortId + 1}`;
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

export class Connection {
  public state: ConnectionState = 'open';
  private receivedListeners = new Set<ReceivedCallback>();
  private messageReceivedListeners = new Set<MessageReceivedCallback>();
  private closedListeners = new Set<ClosedCallback>();

  constructor(public readonly peerSystemId: string, private readonly session: Session) {}

  public async send(bytes: Uint8Array): Promise<void> {
    if (this.state !== 'open') throw new Error('Closed');
    this.session.writeEP1(bytes);
  }

  public async sendMessage(message: Uint8Array): Promise<void> {
    if (this.state !== 'open') throw new Error('Closed');
    // For MVP message framing, prepend a 4-byte big-endian length
    const framed = new Uint8Array(4 + message.length);
    const view = new DataView(framed.buffer);
    view.setUint32(0, message.length);
    framed.set(message, 4);
    this.session.writeEP1(framed);
  }

  public async close(): Promise<void> {
    if (this.state !== 'open') return;
    this.state = 'closing';
    try {
      await this.session.disconnectConnection();
    } finally {
      this.state = 'closed';
      this.closedListeners.forEach(l => l('closed'));
    }
  }

  public handleData(data: Uint8Array): void {
    // Deliver raw bytes
    this.receivedListeners.forEach(l => l(data));

    // Deliver messages if framed (starts with 4-byte length matching the remaining bytes)
    if (data.length >= 4) {
      const view = new DataView(data.buffer, data.byteOffset, 4);
      const msgLen = view.getUint32(0);
      if (msgLen === data.length - 4) {
        this.messageReceivedListeners.forEach(l => l(data.subarray(4)));
      }
    }
  }

  public handleRemoteClose(reason: string): void {
    if (this.state !== 'open') return;
    this.state = 'closed';
    this.session.closeConnectionLocal();
    this.closedListeners.forEach(l => l(reason));
  }

  public onReceived(callback: ReceivedCallback): () => void {
    this.receivedListeners.add(callback);
    return () => this.receivedListeners.delete(callback);
  }

  public onMessageReceived(callback: MessageReceivedCallback): () => void {
    this.messageReceivedListeners.add(callback);
    return () => this.messageReceivedListeners.delete(callback);
  }

  public onClosed(callback: ClosedCallback): () => void {
    this.closedListeners.add(callback);
    return () => this.closedListeners.delete(callback);
  }
}
export { Transport } from './transport';
export { SimTransport } from './transports/sim';
export { UsbTransport } from './transports/usb';
