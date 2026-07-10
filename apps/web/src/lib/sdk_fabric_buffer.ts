import type { ParsedHeader } from './fabric_protocol';
import { concatBytes, takeBytes, takeHeader, waitUntil } from './sdk_fabric_io';

/** Accumulates opaque EP2 payloads for ROCKETBX header + file body. */
export class CircuitDataBuffer {
  current = new Uint8Array(0);

  clear(): void {
    this.current = new Uint8Array(0);
  }

  append(bytes: Uint8Array): void {
    this.current = new Uint8Array(concatBytes(this.current, bytes));
  }

  waitHeader(timeoutMs: number): Promise<ParsedHeader> {
    return waitUntil(() => takeHeader(this), timeoutMs, 'header timeout');
  }

  waitPayload(
    size: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    return waitUntil(() => takeBytes(this, size, onProgress), 120_000, 'payload timeout');
  }
}
