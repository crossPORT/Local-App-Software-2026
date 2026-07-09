import type { Transport } from '../transport';

const MSG_NAK = 0x84;

export class SimTransport implements Transport {
  private socket: WebSocket | null = null;
  private onDataCallback: ((data: Uint8Array) => void) | null = null;
  private onControlCallback: ((header: DataView, payload: Uint8Array) => void) | null = null;
  
  private nextTxn = 1;
  private txnCallbacks = new Map<number, { resolve: (header: DataView, payload: Uint8Array) => void; reject: (err: Error) => void }>();

  constructor(private readonly port: number, private readonly customPort?: number) {}

  get connected(): boolean {
    return this.socket !== null && this.socket.readyState === 1;
  }

  async init(): Promise<void> {
    const wsPort = this.customPort ?? 1773;
    const wsUrl = `ws://localhost:${wsPort}?port=${this.port}`;
    this.socket = new WebSocket(wsUrl);
    this.socket.binaryType = 'arraybuffer';

    await new Promise<void>((resolve, reject) => {
      const onOpen = () => {
        this.socket?.removeEventListener('open', onOpen);
        this.socket?.removeEventListener('error', onError);
        resolve();
      };
      const onError = (err: any) => {
        this.socket?.removeEventListener('open', onOpen);
        this.socket?.removeEventListener('error', onError);
        reject(new Error(`Could not connect to simulation daemon: ${err.message || 'unknown error'}`));
      };
      this.socket?.addEventListener('open', onOpen);
      this.socket?.addEventListener('error', onError);
    });

    this.socket.addEventListener('message', (event) => {
      this.handleIncomingMessage(new Uint8Array(event.data));
    });
  }

  async disconnect(): Promise<void> {
    this.socket?.close();
    this.socket = null;
    this.txnCallbacks.forEach(cb => cb.reject(new Error('detached')));
    this.txnCallbacks.clear();
  }

  private handleIncomingMessage(data: Uint8Array): void {
    if (data.length === 0) return;
    const epId = data[0];
    const packet = data.subarray(1);

    if (epId === 0x03) {
      // EP3 Control IN
      if (packet.length < 12) return;
      const header = new DataView(packet.buffer, packet.byteOffset, 12);
      const payload = packet.subarray(12);

      const txn = header.getUint16(2);
      const type = header.getUint8(1);
      const arg = header.getUint32(4);

      if (this.txnCallbacks.has(txn)) {
        const cb = this.txnCallbacks.get(txn)!;
        this.txnCallbacks.delete(txn);
        if (type === MSG_NAK) {
          let reason = 'busy';
          if (arg === 0x02) reason = 'offline';
          else if (arg === 0x03) reason = 'denied';
          else if (arg === 0x04) reason = 'invalid';
          else if (arg === 0x05) reason = 'timeout';
          cb.reject(new Error(reason));
        } else {
          cb.resolve(header, payload);
        }
      } else {
        if (this.onControlCallback) {
          this.onControlCallback(header, payload);
        }
      }
    } else if (epId === 0x02) {
      // EP2 Data IN
      if (this.onDataCallback && packet.length >= 4) {
        const view = new DataView(packet.buffer, packet.byteOffset, 4);
        const dataLen = view.getUint32(0);
        const dataBytes = packet.subarray(4, 4 + dataLen);
        this.onDataCallback(dataBytes);
      }
    }
  }

  writeEP1(data: Uint8Array): void {
    const packet = new Uint8Array(1 + 4 + data.length);
    packet[0] = 0x01; // EP1 Data OUT
    const view = new DataView(packet.buffer);
    view.setUint32(1, data.length);
    packet.set(data, 5);
    if (this.socket && this.socket.readyState === 1) {
      this.socket.send(packet);
    }
  }

  writeEP4(data: Uint8Array): Promise<[DataView, Uint8Array]> {
    // Extract txn from header
    const headerBytes = data.subarray(1, 13);
    const view = new DataView(headerBytes.buffer, headerBytes.byteOffset, headerBytes.byteLength);
    const txn = view.getUint16(2);

    return new Promise((resolve, reject) => {
      this.txnCallbacks.set(txn, {
        resolve: (h, p) => resolve([h, p]),
        reject,
      });
      if (this.socket && this.socket.readyState === 1) {
        this.socket.send(data);
      } else {
        reject(new Error('detached'));
      }
    });
  }

  onEP2Received(callback: (data: Uint8Array) => void): void {
    this.onDataCallback = callback;
  }

  onEP3Received(callback: (header: DataView, payload: Uint8Array) => void): void {
    this.onControlCallback = callback;
  }
}
