import { RocketBoxError } from '../errors';
import { debugLog } from '../debug_log';
import {
  CHUNK_SIZE,
  HEADER_SIZE,
  concatChunks,
  parseHeader,
} from '../protocol';
import { parseSessionPayload } from '../session_codec';
import type { SessionMessage } from '../session_types';
import type { DataListen } from './data_listen';
import type { UsbEndpoints } from './usb_ids';

type SessionEmit = (m: SessionMessage) => void;

/** ROCKETBX payload IN (skip stray session frames). */
export class DataRecv {
  private usbTail: Promise<void> = Promise.resolve();

  constructor(
    private getDevice: () => USBDevice | null,
    private getEps: () => UsbEndpoints | null,
    private getLeg: () => number,
    private listen: DataListen,
    private onSession: SessionEmit,
  ) {}

  private runUsb<T>(fn: () => Promise<T>): Promise<T> {
    const run = this.usbTail.then(() => fn());
    this.usbTail = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }

  private async readExact(n: number): Promise<Uint8Array> {
    const device = this.getDevice();
    const eps = this.getEps();
    if (!device || !eps) throw new RocketBoxError('USB not connected', 'usb');
    const out = new Uint8Array(n);
    let off = 0;
    while (off < n) {
      const r = await device.transferIn(eps.ep2In, Math.min(CHUNK_SIZE, n - off));
      if (r.status !== 'ok' || !r.data?.byteLength) {
        throw new RocketBoxError(`Payload transferIn failed: ${r.status}`, 'usb');
      }
      const part = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
      const take = Math.min(part.length, n - off);
      out.set(part.subarray(0, take), off);
      off += take;
    }
    return out;
  }

  async receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    return this.runUsb(async () => {
      const device = this.getDevice();
      const eps = this.getEps();
      if (!device || !eps) throw new RocketBoxError('USB not connected', 'usb');
      debugLog(this.getLeg(), 'payload_recv_arm', `timeout_ms=${headerTimeoutMs}`);
      this.listen.beginOutbound();
      try {
        const deadline = Date.now() + headerTimeoutMs;
        let skipped = 0;
        while (Date.now() < deadline && skipped < 16) {
          const remaining = Math.max(1, deadline - Date.now());
          const headerBuf = await Promise.race([
            this.readExact(HEADER_SIZE),
            new Promise<never>((_, rej) => {
              window.setTimeout(() => rej(new RocketBoxError('payload header timeout', 'timeout')), remaining);
            }),
          ]);
          const header = parseHeader(headerBuf);
          if (header.frameKind === 'session') {
            const data = await this.readExact(header.fileSize);
            const parsed = parseSessionPayload(data);
            if (parsed) {
              debugLog(this.getLeg(), 'stray_session_skipped', parsed.kind);
              this.onSession(parsed);
            }
            skipped += 1;
            continue;
          }
          if (header.frameKind !== 'payload' || header.fileSize === 0) {
            debugLog(this.getLeg(), 'payload_hdr_skip', header.frameKind);
            skipped += 1;
            continue;
          }
          if (expectedBytes > 0 && header.fileSize !== expectedBytes) {
            throw new RocketBoxError(
              `Expected ${expectedBytes} byte payload but header announced ${header.fileSize}`,
              'protocol',
            );
          }
          const track = expectedBytes > 0 && header.fileSize === expectedBytes;
          const parts: Uint8Array[] = [];
          let received = 0;
          while (received < header.fileSize) {
            const need = Math.min(CHUNK_SIZE, header.fileSize - received);
            const chunk = await this.readExact(need);
            parts.push(chunk);
            received += chunk.length;
            if (track) onProgress?.(received, header.fileSize);
          }
          debugLog(this.getLeg(), 'payload_recv_ok', String(header.fileSize));
          return {
            data: concatChunks(parts, header.fileSize),
            filename: header.filename || 'download.bin',
          };
        }
        throw new RocketBoxError('payload header timeout', 'timeout');
      } catch (err) {
        debugLog(this.getLeg(), 'payload_recv_fail', (err as Error).message);
        throw err;
      } finally {
        this.listen.endOutbound();
      }
    });
  }
}
