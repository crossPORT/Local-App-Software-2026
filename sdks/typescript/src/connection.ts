export type ConnectionState = 'open' | 'closing' | 'closed';

type ReceivedCallback = (bytes: Uint8Array) => void;
type MessageReceivedCallback = (message: Uint8Array) => void;
type ClosedCallback = (reason: string) => void;

/** Minimal session surface Connection needs (avoids circular import). */
export interface ConnectionSession {
  writeEP1(bytes: Uint8Array): void;
  disconnectConnection(): Promise<void>;
  closeConnectionLocal(): void;
}

export class Connection {
  public state: ConnectionState = 'open';
  private receivedListeners = new Set<ReceivedCallback>();
  private messageReceivedListeners = new Set<MessageReceivedCallback>();
  private closedListeners = new Set<ClosedCallback>();

  constructor(public readonly peerSystemId: string, private readonly session: ConnectionSession) {}

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
