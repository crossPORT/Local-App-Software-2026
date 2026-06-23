import { CHUNK_SIZE, HEADER_SIZE, buildHeader, concatChunks, parseHeader } from '../src/lib/fabric_protocol';
import type { FabricTransport } from '../src/lib/fabric_transport';
import {
  type FabricSessionMessage,
  parseSessionPayload,
  serializeSessionMessage,
} from '../src/lib/fabric_session';
import { FabricUsbError } from '../src/lib/fabric_usb';
import { MAX_SESSION_FILE_BYTES, PAYLOAD_HEADER_TIMEOUT_MS } from '../src/lib/usb_constants';
import { fabricHubReceiveBuffer, fabricHubReset, fabricHubTransmit } from './fabric_hub';

function simStorageKey(portIndex: number): string {
  return `rocketbox_sim_connected_port${portIndex}`;
}

export class FabricSimSession implements FabricTransport {
  private linked = false;

  constructor(private readonly portIndex: number) {}

  getFabricPortIndex(): number {
    return this.portIndex;
  }

  get connected(): boolean {
    return this.linked;
  }

  static async countFabricDevices(): Promise<number> {
    return 2;
  }

  static hasSavedSerial(portIndex: number): boolean {
    return sessionStorage.getItem(simStorageKey(portIndex)) === '1';
  }

  async connect(): Promise<string> {
    fabricHubReset();
    this.linked = true;
    sessionStorage.setItem(simStorageKey(this.portIndex), '1');
    return this.describeDevice();
  }

  async reconnectKnown(): Promise<string> {
    if (!FabricSimSession.hasSavedSerial(this.portIndex)) {
      throw new FabricUsbError('No saved sim link — click Connect');
    }
    this.linked = true;
    return this.describeDevice();
  }

  describeDevice(): string {
    return `Simulated device · port ${this.portIndex}`;
  }

  async disconnect(): Promise<void> {
    this.linked = false;
  }

  async forgetThisDevice(): Promise<void> {
    await this.disconnect();
    sessionStorage.removeItem(simStorageKey(this.portIndex));
    fabricHubReset();
  }

  async resetConnection(): Promise<string> {
    if (!this.linked) {
      throw new FabricUsbError('Sim not connected');
    }
    fabricHubReceiveBuffer(this.portIndex).clear();
    return this.describeDevice();
  }

  ownsDevice(_usbDevice: USBDevice): boolean {
    return false;
  }

  markDisconnected(): void {
    this.linked = false;
  }

  async sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    if (!this.linked) {
      throw new FabricUsbError('Sim not connected');
    }
    fabricHubTransmit(this.portIndex, new Uint8Array(buildHeader(payload.length, filename)));
    onProgress?.(0, payload.length);

    let offset = 0;
    while (offset < payload.length) {
      const chunk = new Uint8Array(CHUNK_SIZE);
      const n = Math.min(payload.length - offset, CHUNK_SIZE);
      chunk.set(payload.subarray(offset, offset + n));
      fabricHubTransmit(this.portIndex, chunk);
      offset += n;
      onProgress?.(offset, payload.length);
    }
  }

  async receiveHeader(): Promise<{ fileSize: number; filename: string }> {
    if (!this.linked) {
      throw new FabricUsbError('Sim not connected');
    }
    const headerBuf = await fabricHubReceiveBuffer(this.portIndex).readExact(
      HEADER_SIZE,
      PAYLOAD_HEADER_TIMEOUT_MS,
    );
    return parseHeader(headerBuf);
  }

  async receivePayload(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    if (!this.linked) {
      throw new FabricUsbError('Sim not connected');
    }
    const parts: Uint8Array[] = [];
    let received = 0;
    while (received < fileSize) {
      const wireChunk = await fabricHubReceiveBuffer(this.portIndex).readUpTo(
        CHUNK_SIZE,
        PAYLOAD_HEADER_TIMEOUT_MS,
      );
      const take = Math.min(wireChunk.length, fileSize - received);
      parts.push(wireChunk.subarray(0, take));
      received += take;
      onProgress?.(received, fileSize);
    }
    return concatChunks(parts, fileSize);
  }

  async discardPayload(fileSize: number): Promise<void> {
    await this.receivePayload(fileSize);
  }

  async receiveBytes(onProgress?: (done: number, total: number) => void): Promise<Uint8Array> {
    const { fileSize } = await this.receiveHeader();
    return this.receivePayload(fileSize, onProgress);
  }

  async sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    await this.sendBytes(serializeSessionMessage(message), undefined, '');
  }

  async tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    if (!this.linked) {
      return null;
    }

    let timeoutId: ReturnType<typeof setTimeout> | undefined;
    const headerPromise = this.receiveHeader().then(async ({ fileSize }) => {
      if (fileSize === 0 || fileSize > MAX_SESSION_FILE_BYTES) {
        if (fileSize > MAX_SESSION_FILE_BYTES) {
          await this.receivePayload(fileSize).catch(() => {});
        }
        throw new FabricUsbError('Not a session message');
      }
      const data = await this.receivePayload(fileSize);
      const message = parseSessionPayload(data);
      if (!message) {
        throw new FabricUsbError('Session parse failed');
      }
      return message;
    });

    try {
      const raced = await Promise.race([
        headerPromise.then((message) => ({ type: 'msg' as const, message })),
        new Promise<{ type: 'timeout' }>((resolve) => {
          timeoutId = window.setTimeout(() => resolve({ type: 'timeout' }), headerTimeoutMs);
        }),
      ]);
      if (timeoutId !== undefined) {
        window.clearTimeout(timeoutId);
      }
      if (raced.type === 'timeout') {
        return null;
      }
      return raced.message;
    } catch {
      if (timeoutId !== undefined) {
        window.clearTimeout(timeoutId);
      }
      await this.resetConnection().catch(() => {});
      return null;
    }
  }

  async receiveFileTransfer(
    headerTimeoutMs: number,
    _expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    if (!this.linked) {
      throw new FabricUsbError('Sim not connected');
    }

    let timeoutId: ReturnType<typeof setTimeout> | undefined;
    try {
      const header = await Promise.race([
        this.receiveHeader(),
        new Promise<never>((_, reject) => {
          timeoutId = window.setTimeout(
            () => reject(new FabricUsbError('Payload header timeout')),
            headerTimeoutMs,
          );
        }),
      ]);
      if (timeoutId !== undefined) {
        window.clearTimeout(timeoutId);
      }
      const data = await this.receivePayload(header.fileSize, onProgress);
      return {
        data,
        filename: header.filename || 'download.bin',
      };
    } catch (err) {
      if (timeoutId !== undefined) {
        window.clearTimeout(timeoutId);
      }
      throw err;
    }
  }

  async prepareForPayloadSend(): Promise<void> {
    /* Sim has no runUsb queue — no-op. */
  }
}
