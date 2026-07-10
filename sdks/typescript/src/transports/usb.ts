import type { Transport } from '../transport';
import { USB_READ_SIZE, type UsbEndpoints } from './usb_ids';
import { openAndClaim, pickDevice, releaseDevice } from './usb_open';
import {
  concatBytes,
  drainEp2,
  drainEp3,
  type UsbTxnCb,
} from './usb_rx';

export interface UsbTransportOptions {
  /** Pre-selected WebUSB device (skips picker when set). */
  device?: USBDevice;
}

const EP4_TIMEOUT_MS = 5_000;

/** WebUSB transport for the IntelliConnex 4-endpoint Session protocol. */
export class UsbTransport implements Transport {
  private device: USBDevice | null = null;
  private eps: UsbEndpoints | null = null;
  private running = false;
  private onData: ((data: Uint8Array) => void) | null = null;
  private onControl: ((header: DataView, payload: Uint8Array) => void) | null = null;
  private onDisconnectCb: (() => void) | null = null;
  private txnCallbacks = new Map<number, UsbTxnCb>();
  private ep2Buf: Uint8Array = new Uint8Array(0);
  private ep3Buf: Uint8Array = new Uint8Array(0);
  private onUsbDisconnect: ((ev: USBConnectionEvent) => void) | null = null;

  constructor(private readonly options: UsbTransportOptions = {}) {}

  get connected(): boolean {
    return this.device?.opened === true && this.running;
  }

  onDisconnected(callback: () => void): void {
    this.onDisconnectCb = callback;
  }

  async init(): Promise<void> {
    this.device = await pickDevice(this.options.device);
    this.eps = await openAndClaim(this.device);
    this.running = true;
    this.bindUsbDisconnect();
    void this.readLoopEp2();
    void this.readLoopEp3();
  }

  async disconnect(): Promise<void> {
    this.onDisconnectCb = null;
    this.unbindUsbDisconnect();
    this.running = false;
    this.failPending('detached');
    await releaseDevice(this.device);
    this.device = null;
    this.eps = null;
  }

  private bindUsbDisconnect(): void {
    const usb = navigator.usb;
    if (!usb || !this.device) return;
    const watched = this.device;
    this.onUsbDisconnect = (ev: USBConnectionEvent) => {
      if (ev.device === watched) this.failDetached();
    };
    usb.addEventListener('disconnect', this.onUsbDisconnect);
  }

  private unbindUsbDisconnect(): void {
    if (this.onUsbDisconnect) {
      navigator.usb?.removeEventListener('disconnect', this.onUsbDisconnect);
      this.onUsbDisconnect = null;
    }
  }

  private failDetached(): void {
    if (!this.running) return;
    this.running = false;
    this.unbindUsbDisconnect();
    this.failPending('detached');
    this.onDisconnectCb?.();
  }

  private failPending(reason: string): void {
    this.txnCallbacks.forEach((cb) => cb.reject(new Error(reason)));
    this.txnCallbacks.clear();
  }

  writeEP1(data: Uint8Array): void {
    if (!this.device || !this.eps) return;
    const packet = new Uint8Array(4 + data.length);
    new DataView(packet.buffer).setUint32(0, data.length);
    packet.set(data, 4);
    void this.device.transferOut(this.eps.ep1Out, packet);
  }

  writeEP4(data: Uint8Array): Promise<[DataView, Uint8Array]> {
    if (!this.device || !this.eps) return Promise.reject(new Error('detached'));
    const wire = data.length > 0 && data[0] === 0x04 ? data.subarray(1) : data;
    if (wire.length < 12) return Promise.reject(new Error('invalid control packet'));
    const txn = new DataView(wire.buffer, wire.byteOffset, 12).getUint16(2);

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
      void this.device!.transferOut(this.eps!.ep4Out, wire.slice()).catch((err) => {
        clearTimeout(timer);
        this.txnCallbacks.delete(txn);
        reject(err instanceof Error ? err : new Error(String(err)));
      });
    });
  }

  onEP2Received(callback: (data: Uint8Array) => void): void {
    this.onData = callback;
  }

  onEP3Received(callback: (header: DataView, payload: Uint8Array) => void): void {
    this.onControl = callback;
  }

  private async readLoopEp2(): Promise<void> {
    while (this.running && this.device && this.eps) {
      try {
        const r = await this.device.transferIn(this.eps.ep2In, USB_READ_SIZE);
        if (r.status !== 'ok' || !r.data || r.data.byteLength === 0) continue;
        const chunk = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
        this.ep2Buf = drainEp2(concatBytes(this.ep2Buf, chunk), this.onData);
      } catch {
        if (!this.running) break;
        this.failDetached();
        break;
      }
    }
  }

  private async readLoopEp3(): Promise<void> {
    while (this.running && this.device && this.eps) {
      try {
        const r = await this.device.transferIn(this.eps.ep3In, USB_READ_SIZE);
        if (r.status !== 'ok' || !r.data || r.data.byteLength === 0) continue;
        const chunk = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
        this.ep3Buf = drainEp3(
          concatBytes(this.ep3Buf, chunk),
          this.txnCallbacks,
          this.onControl,
        );
      } catch {
        if (!this.running) break;
        this.failDetached();
        break;
      }
    }
  }
}
