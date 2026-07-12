import { fabricDebugLog } from '../fabric/debug_log';
import { FabricUsbError } from '../fabric/errors';
import { HEADER_SIZE, buildHeader, parseHeader } from '../fabric/protocol';
import { parseSessionPayload } from '../fabric/session_codec';
import type { FabricSessionMessage } from '../fabric/session_types';
import type { ListenMode } from '../fabric/types';
import { isBenignListenError, sleep } from './bulk_io';
import type { UsbEndpoints } from './usb_ids';

const ALWAYS_HEADER_MS = 200;
const LISTEN_GAP_MS = 250;
const HANDSHAKE_GAP_MS = 50;

type SessionCb = (m: FabricSessionMessage) => void;

/** Background ROCKETBX session listener (C++ SessionListener parity). */
export class DataListen {
  private mode: ListenMode = 'off';
  private generation = 0;
  private running = false;
  private suspended = 0;
  private pendingHeader: Promise<USBInTransferResult | null> | null = null;
  private activeIn: Promise<void> = Promise.resolve();
  private readonly handlers = new Set<SessionCb>();
  private handshakeMs = 350;

  constructor(
    private getDevice: () => USBDevice | null,
    private getEps: () => UsbEndpoints | null,
    private getLeg: () => number,
  ) {}

  setHandshakePollTimeoutMs(ms: number): void {
    this.handshakeMs = ms;
  }

  setListenMode(mode: ListenMode): void {
    const prev = this.mode;
    this.mode = mode;
    fabricDebugLog(this.getLeg(), 'listen_mode', mode);
    if (mode === 'off') this.stop();
    else if (prev === 'off' || !this.running) this.start();
  }

  ensureListening(): void {
    if (this.mode !== 'off' && !this.running) {
      fabricDebugLog(this.getLeg(), 'listen_restart', 'watchdog');
      this.start();
    }
  }

  subscribe(cb: SessionCb): () => void {
    this.handlers.add(cb);
    return () => this.handlers.delete(cb);
  }

  beginOutbound(): void {
    this.suspended += 1;
    this.stop();
  }

  endOutbound(): void {
    this.suspended = Math.max(0, this.suspended - 1);
    if (this.mode !== 'off') {
      fabricDebugLog(this.getLeg(), 'listen_restart', this.mode);
      this.start();
    }
  }

  getActiveInPoll(): Promise<void> {
    return this.activeIn;
  }

  clearPendingHeader(): void {
    this.pendingHeader = null;
  }

  start(): void {
    this.stop();
    const gen = ++this.generation;
    this.running = true;
    void this.loop(gen).finally(() => {
      if (this.generation === gen) this.running = false;
    });
  }

  stop(): void {
    this.generation += 1;
    this.running = false;
  }

  private emit(m: FabricSessionMessage): void {
    for (const h of this.handlers) h(m);
  }

  private headerRead(device: USBDevice, epIn: number): Promise<USBInTransferResult | null> {
    if (this.pendingHeader) return this.pendingHeader;
    const read = device.transferIn(epIn, HEADER_SIZE).then(
      (r) => r,
      (err) => {
        if (!isBenignListenError(err) && (err as Error)?.name !== 'AbortError') {
          fabricDebugLog(this.getLeg(), 'usb_recv_fail', (err as Error).message);
        }
        return null;
      },
    );
    this.pendingHeader = read;
    return read;
  }

  private async tryOnce(headerMs: number): Promise<FabricSessionMessage | null> {
    const device = this.getDevice();
    const eps = this.getEps();
    if (!device || !eps) return null;
    try {
      const headerRead = this.headerRead(device, eps.ep2In);
      const raced = await Promise.race([
        headerRead.then((result) => ({ ready: true as const, result })),
        sleep(headerMs).then(() => ({ ready: false as const, result: null })),
      ]);
      if (!raced.ready) return null;
      this.clearPendingHeader();
      const result = raced.result;
      if (!result || result.status !== 'ok' || !result.data?.byteLength) return null;
      const chunk = new Uint8Array(result.data.buffer, result.data.byteOffset, result.data.byteLength);
      if (chunk.length < HEADER_SIZE) return null;
      const header = parseHeader(chunk.subarray(0, HEADER_SIZE));
      if (header.frameKind !== 'session') {
        if (header.fileSize > 0) await this.discardBody(device, eps.ep2In, header.fileSize);
        return null;
      }
      if (header.fileSize <= 0 || header.fileSize > 256 * 1024) return null;
      const body = await this.readExact(device, eps.ep2In, header.fileSize);
      return parseSessionPayload(body);
    } catch {
      this.clearPendingHeader();
      return null;
    }
  }

  private async readExact(device: USBDevice, epIn: number, n: number): Promise<Uint8Array> {
    const out = new Uint8Array(n);
    let off = 0;
    while (off < n) {
      const r = await device.transferIn(epIn, Math.min(16 * 1024, n - off));
      if (r.status !== 'ok' || !r.data?.byteLength) throw new FabricUsbError('session body read failed');
      const part = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
      out.set(part.subarray(0, n - off), off);
      off += part.length;
    }
    return out;
  }

  private async discardBody(device: USBDevice, epIn: number, n: number): Promise<void> {
    let left = n;
    while (left > 0) {
      const r = await device.transferIn(epIn, Math.min(16 * 1024, left));
      if (r.status !== 'ok' || !r.data?.byteLength) break;
      left -= r.data.byteLength;
    }
  }

  private async loop(gen: number): Promise<void> {
    while (gen === this.generation && this.mode !== 'off') {
      const headerMs =
        this.mode === 'always' ? Math.min(ALWAYS_HEADER_MS, this.handshakeMs) : this.handshakeMs;
      const gap = this.mode === 'handshake' ? HANDSHAKE_GAP_MS : LISTEN_GAP_MS;
      if (this.suspended === 0) {
        const poll = this.tryOnce(headerMs);
        this.activeIn = poll.then(
          () => undefined,
          () => undefined,
        );
        const message = await poll;
        if (message) {
          fabricDebugLog(this.getLeg(), 'session_frame', message.kind);
          this.emit(message);
        }
      }
      if (gen !== this.generation) return;
      await sleep(gap);
    }
  }
}
