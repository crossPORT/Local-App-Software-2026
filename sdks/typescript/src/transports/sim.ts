import type { Transport } from '../transport';
import { MSG_NAK } from '../messages';

const EP4_TIMEOUT_MS = 5_000;

type TxnCb = {
  resolve: (header: DataView, payload: Uint8Array) => void;
  reject: (err: Error) => void;
};

export class SimTransport implements Transport {
  private socket: WebSocket | null = null;
  private onDataCallback: ((data: Uint8Array) => void) | null = null;
  private onControlCallback: ((header: DataView, payload: Uint8Array) => void) | null = null;
  private onDisconnectCb: (() => void) | null = null;
  private txnCallbacks = new Map<number, TxnCb>();

  constructor(private readonly port: number, private readonly customPort?: number) {}

  get connected(): boolean {
    return this.socket !== null && this.socket.readyState === 1;
  }

  onDisconnected(callback: () => void): void {
    this.onDisconnectCb = callback;
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
      const onError = (err: Event) => {
        this.socket?.removeEventListener('open', onOpen);
        this.socket?.removeEventListener('error', onError);
        const msg = err instanceof ErrorEvent ? err.message : 'unknown error';
        reject(new Error(`Could not connect to simulation daemon: ${msg}`));
      };
      this.socket?.addEventListener('open', onOpen);
      this.socket?.addEventListener('error', onError);
    });

    this.socket.addEventListener('message', (event) => {
      this.handleIncomingMessage(new Uint8Array(event.data as ArrayBuffer));
    });
    this.socket.addEventListener('close', () => this.handleSocketClosed());
  }

  async disconnect(): Promise<void> {
    this.onDisconnectCb = null;
    this.socket?.close();
    this.failPending('detached');
    this.socket = null;
  }

  private handleSocketClosed(): void {
    if (this.socket === null) return;
    this.socket = null;
    this.failPending('detached');
    this.onDisconnectCb?.();
  }

  private failPending(reason: string): void {
    this.txnCallbacks.forEach((cb) => cb.reject(new Error(reason)));
    this.txnCallbacks.clear();
  }

  private handleIncomingMessage(data: Uint8Array): void {
    if (data.length === 0) return;
    const epId = data[0];
    const packet = data.subarray(1);

    if (epId === 0x03) {
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
          cb.reject(new Error(nakReason(arg)));
        } else {
          cb.resolve(header, payload);
        }
      } else {
        this.onControlCallback?.(header, payload);
      }
    } else if (epId === 0x02 && this.onDataCallback && packet.length >= 4) {
      const view = new DataView(packet.buffer, packet.byteOffset, 4);
      const dataLen = view.getUint32(0);
      this.onDataCallback(packet.subarray(4, 4 + dataLen));
    }
  }

  writeEP1(data: Uint8Array): void {
    const packet = new Uint8Array(1 + 4 + data.length);
    packet[0] = 0x01;
    new DataView(packet.buffer).setUint32(1, data.length);
    packet.set(data, 5);
    if (this.socket?.readyState === 1) this.socket.send(packet);
  }

  writeEP4(data: Uint8Array): Promise<[DataView, Uint8Array]> {
    const headerBytes = data.subarray(1, 13);
    const view = new DataView(headerBytes.buffer, headerBytes.byteOffset, headerBytes.byteLength);
    const txn = view.getUint16(2);

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (!this.txnCallbacks.has(txn)) return;
        this.txnCallbacks.delete(txn);
        reject(new Error('timeout'));
      }, EP4_TIMEOUT_MS);
      this.txnCallbacks.set(txn, {
        resolve: (h, p) => {
          clearTimeout(timer);
          resolve([h, p]);
        },
        reject: (err) => {
          clearTimeout(timer);
          reject(err);
        },
      });
      if (this.socket?.readyState === 1) {
        this.socket.send(data);
      } else {
        clearTimeout(timer);
        this.txnCallbacks.delete(txn);
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

function nakReason(arg: number): string {
  if (arg === 0x02) return 'offline';
  if (arg === 0x03) return 'denied';
  if (arg === 0x04) return 'invalid';
  if (arg === 0x05) return 'timeout';
  return 'busy';
}
