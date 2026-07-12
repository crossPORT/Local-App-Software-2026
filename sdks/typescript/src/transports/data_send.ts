import { fabricDebugLog } from '../fabric/debug_log';
import { FabricUsbError } from '../fabric/errors';
import { CHUNK_SIZE, buildHeader } from '../fabric/protocol';
import { serializeSessionMessage } from '../fabric/session_codec';
import type { FabricSessionMessage } from '../fabric/session_types';
import { sleep, transferOutWithRetry } from './bulk_io';
import type { DataListen } from './data_listen';
import type { UsbEndpoints } from './usb_ids';
import { INTERFACE_NUMBER } from './usb_ids';

const SESSION_OUT_MS = 2500;
const PAYLOAD_CHUNK_MS = 30_000;
const IN_SETTLE_MS = 50;
const ACTIVE_IN_WAIT_MS = 400;
const MAX_SESSION_BYTES = 256 * 1024;

/** ROCKETBX session/payload OUT (C++ send_file_core parity). */
export class DataSend {
  private usbTail: Promise<void> = Promise.resolve();
  private sessionTail: Promise<void> = Promise.resolve();
  private ifaceRecovery: Promise<void> = Promise.resolve();

  constructor(
    private getDevice: () => USBDevice | null,
    private getEps: () => UsbEndpoints | null,
    private getLeg: () => number,
    private listen: DataListen,
  ) {}

  async waitForIdle(): Promise<void> {
    await this.usbTail;
  }

  async prepareForPayloadSend(): Promise<void> {
    await this.waitForIdle();
    const device = this.getDevice();
    const eps = this.getEps();
    if (device && eps) await this.clearHalts(device, eps);
  }

  private runUsb<T>(fn: () => Promise<T>): Promise<T> {
    const run = this.usbTail.then(() => fn());
    this.usbTail = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }

  private async clearHalts(device: USBDevice, eps: UsbEndpoints): Promise<void> {
    if (typeof device.clearHalt !== 'function') return;
    for (const [dir, num] of [
      ['out', eps.ep1Out],
      ['in', eps.ep2In],
    ] as const) {
      try {
        await device.clearHalt(dir, num);
      } catch {
        /* best effort */
      }
    }
  }

  private async reclaim(device: USBDevice, eps: UsbEndpoints): Promise<void> {
    this.listen.clearPendingHeader();
    try {
      await device.releaseInterface(INTERFACE_NUMBER);
    } catch {
      /* best effort */
    }
    if (device.configuration == null) await device.selectConfiguration(1);
    await device.claimInterface(INTERFACE_NUMBER);
    await this.clearHalts(device, eps);
  }

  async abortStuck(): Promise<void> {
    const device = this.getDevice();
    const eps = this.getEps();
    if (!device?.opened || !eps) return;
    const recovery = this.ifaceRecovery.then(
      () => this.reclaim(device, eps),
      () => this.reclaim(device, eps),
    );
    this.ifaceRecovery = recovery.then(
      () => undefined,
      () => undefined,
    );
    // Must wait for reclaim to finish — racing a timeout left reclaim in-flight
    // and the next transferOut hung (payload header after ready).
    await recovery;
  }

  private async prepareBus(): Promise<void> {
    fabricDebugLog(this.getLeg(), 'session_send_prep', 'wait_in');
    await Promise.race([this.listen.getActiveInPoll(), sleep(ACTIVE_IN_WAIT_MS)]);
    // Full reclaim cancels any outstanding transferIn — clearHalt alone left IN
    // pending and peers often missed announces on the no-buffer fabric.
    fabricDebugLog(this.getLeg(), 'session_send_prep', 'abort_iface');
    await this.abortStuck();
    await sleep(IN_SETTLE_MS);
    fabricDebugLog(this.getLeg(), 'session_send_prep', 'ready');
  }

  /** Light prep for payload — avoid iface reclaim after handshake. */
  private async preparePayloadBus(device: USBDevice, eps: UsbEndpoints): Promise<void> {
    fabricDebugLog(this.getLeg(), 'payload_send_prep', 'wait_in');
    await Promise.race([this.listen.getActiveInPoll(), sleep(ACTIVE_IN_WAIT_MS)]);
    await this.clearHalts(device, eps);
    await sleep(IN_SETTLE_MS);
    fabricDebugLog(this.getLeg(), 'payload_send_prep', 'ready');
  }

  async sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    const bytes = serializeSessionMessage(message);
    if (bytes.length > MAX_SESSION_BYTES) throw new FabricUsbError('Session message too large');
    const run = this.sessionTail.then(
      () => this.runSessionSend(message, bytes),
      () => this.runSessionSend(message, bytes),
    );
    this.sessionTail = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }

  private async runSessionSend(message: FabricSessionMessage, bytes: Uint8Array): Promise<void> {
    await this.runUsb(async () => {
      const device = this.getDevice();
      const eps = this.getEps();
      if (!device || !eps) throw new FabricUsbError('USB not connected');
      const recover = () => this.abortStuck();
      this.listen.beginOutbound();
      try {
        await this.prepareBus();
        await transferOutWithRetry(
          device,
          eps.ep1Out,
          buildHeader(bytes.length, { frameKind: 'session' }),
          SESSION_OUT_MS,
          recover,
        );
        await transferOutWithRetry(device, eps.ep1Out, bytes, SESSION_OUT_MS, recover);
        fabricDebugLog(this.getLeg(), 'session_sent', message.kind);
      } finally {
        this.listen.endOutbound();
      }
    });
  }

  async sendPayload(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    await this.runUsb(async () => {
      const device = this.getDevice();
      const eps = this.getEps();
      if (!device || !eps) throw new FabricUsbError('USB not connected');
      const recover = () => this.abortStuck();
      this.listen.beginOutbound();
      try {
        await this.preparePayloadBus(device, eps);
        fabricDebugLog(this.getLeg(), 'payload_header_out', `bytes=${payload.length}`);
        await transferOutWithRetry(
          device,
          eps.ep1Out,
          buildHeader(payload.length, { frameKind: 'payload', filename }),
          PAYLOAD_CHUNK_MS,
          recover,
        );
        fabricDebugLog(this.getLeg(), 'payload_header_ok', String(payload.length));
        onProgress?.(0, payload.length);
        let offset = 0;
        while (offset < payload.length) {
          // C++ submits wire length `n` only (not a padded 4 MiB buffer).
          const n = Math.min(payload.length - offset, CHUNK_SIZE);
          const chunk = payload.subarray(offset, offset + n);
          await transferOutWithRetry(device, eps.ep1Out, chunk, PAYLOAD_CHUNK_MS, recover);
          offset += n;
          onProgress?.(offset, payload.length);
        }
        fabricDebugLog(this.getLeg(), 'payload_sent', String(payload.length));
      } finally {
        this.listen.endOutbound();
      }
    });
  }
}
