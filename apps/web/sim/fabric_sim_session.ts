import { boothLog } from '../src/lib/booth_log';
import { FabricUsbError } from '../src/lib/fabric_errors';
import type { FabricSessionMessage } from '../src/lib/fabric_session';
import { serializeSessionMessage, parseSessionPayload } from '../src/lib/fabric_session';
import type { FabricTransport } from '../src/lib/fabric_transport';
import type { ParsedHeader } from '../src/lib/fabric_protocol';
import { parseHeader, buildHeader } from '../src/lib/fabric_protocol';
import { formatFabricLegLabel, formatFabricPortDisplay } from '../src/lib/fabric_port';

function parseSystemsPayload(payload: Uint8Array): Array<{ systemId: string; name: string; status: number }> {
  if (payload.length < 4) return [];
  const view = new DataView(payload.buffer, payload.byteOffset, 4);
  const count = view.getUint32(0);
  const list: Array<{ systemId: string; name: string; status: number }> = [];
  let offset = 4;
  for (let i = 0; i < count; i++) {
    if (offset >= payload.length) break;
    const sysIdLen = payload[offset++];
    if (offset + sysIdLen > payload.length) break;
    const systemId = new TextDecoder().decode(payload.subarray(offset, offset + sysIdLen));
    offset += sysIdLen;

    if (offset >= payload.length) break;
    const nameLen = payload[offset++];
    if (offset + nameLen > payload.length) break;
    const name = new TextDecoder().decode(payload.subarray(offset, offset + nameLen));
    offset += nameLen;

    if (offset >= payload.length) break;
    const status = payload[offset++];
    list.push({ systemId, name, status });
  }
  return list;
}

export class FabricSimSession implements FabricTransport {
  private socket: WebSocket | null = null;
  private portIndex = 0;
  private targetPortIndex = 0;
  private sessionHandlers = new Set<(message: FabricSessionMessage) => void>();
  private connectHandlers = new Set<() => void>();
  private onAttachedResolve: ((portIdx: number) => void) | null = null;
  private lastSystemsList: Array<{ systemId: string; name: string; status: number }> = [];

  private pendingPayloads: Array<{ data: Uint8Array; filename: string }> = [];
  private payloadWaiters: Array<{
    resolve: (payload: { data: Uint8Array; filename: string }) => void;
    reject: (err: Error) => void;
  }> = [];

  private receivingFileBuffer: Uint8Array | null = null;
  private receivingFileBytesReceived = 0;
  private receivingFileTotalBytes = 0;
  private receivingFilename = '';
  private onProgressCallback: ((done: number, total: number) => void) | null = null;

  getFabricPortIndex(): number {
    return this.portIndex;
  }

  getFabricLeg(): number {
    return this.portIndex;
  }

  getSerialNumber(): string {
    return `000000000000000${this.portIndex + 1}`;
  }

  get connected(): boolean {
    return this.socket !== null && this.socket.readyState === 1;
  }

  static async countFabricDevices(): Promise<number> {
    return 4;
  }

  static hasSavedSerial(): boolean {
    return false;
  }

  setListenMode(_mode: any): void {}
  ensureListening(): void {}

  subscribeSession(handler: (message: FabricSessionMessage) => void): () => void {
    this.sessionHandlers.add(handler);
    return () => this.sessionHandlers.delete(handler);
  }

  subscribeConnect(handler: () => void): () => void {
    this.connectHandlers.add(handler);
    return () => this.connectHandlers.delete(handler);
  }

  async waitForIdle(): Promise<void> {}
  async prepareForPayloadSend(): Promise<void> {}

  async connect(): Promise<string> {
    const params = new URLSearchParams(window.location.search);
    const urlPort = parseInt(params.get('port') ?? '0', 10);
    let portNum = 0;

    if (urlPort >= 1 && urlPort <= 4) {
      portNum = urlPort;
    }

    const wsUrl = portNum > 0 
      ? `ws://localhost:1773?port=${portNum}` 
      : `ws://localhost:1773?port=auto`;

    this.socket = new WebSocket(wsUrl);
    this.socket.binaryType = 'arraybuffer';

    await new Promise<void>((resolve, reject) => {
      const onOpen = () => {
        this.socket?.removeEventListener('open', onOpen);
        this.socket?.removeEventListener('error', onError);
        resolve();
      };
      const onError = () => {
        this.socket?.removeEventListener('open', onOpen);
        this.socket?.removeEventListener('error', onError);
        this.socket = null;
        reject(new FabricUsbError('Could not connect to simulation daemon — make sure "node daemon.js" is running!'));
      };
      this.socket?.addEventListener('open', onOpen);
      this.socket?.addEventListener('error', onError);
    });

    const attachedPromise = new Promise<number>((resolve) => {
      this.onAttachedResolve = resolve;
    });

    this.socket.addEventListener('message', (event) => {
      this.handleIncomingPacket(new Uint8Array(event.data));
    });

    const socketInstance = this.socket;
    this.socket.addEventListener('close', (event) => {
      this.handleClose(socketInstance, event.code);
    });

    // Wait to receive MSG_ATTACHED to find out what port index we were actually assigned
    const assignedPortIdx = await attachedPromise;
    this.portIndex = assignedPortIdx;

    boothLog(
      this.portIndex,
      'usb_connect',
      formatFabricLegLabel(this.portIndex, this.getSerialNumber()),
    );

    this.connectHandlers.forEach((handler) => {
      try {
        handler();
      } catch (err) {
        console.error('[Sim SDK] Error in connect handler:', err);
      }
    });

    return this.describeDevice();
  }

  private handleIncomingPacket(data: Uint8Array): void {
    if (data.length === 0) return;
    const epId = data[0];
    const packet = data.subarray(1);

    if (epId === 0x03) {
      // EP3 Control IN
      if (packet.length < 12) return;
      const type = packet[1];
      const view = new DataView(packet.buffer, packet.byteOffset, 12);
      const arg = view.getUint32(4);

      if (type === 0x81) { // MSG_ATTACHED
        console.log(`%c[Sim SDK IN] MSG_ATTACHED: assigned to Port ${arg + 1}`, 'color: #00ff00; font-weight: bold;');
        if (this.onAttachedResolve) {
          this.onAttachedResolve(arg);
          this.onAttachedResolve = null;
        }
      }

      const payload = packet.subarray(12);

      if (type === 0x82) {
        const list = parseSystemsPayload(payload);
        console.log(`%c[Sim SDK IN] MSG_SYSTEMS: received online list (Count: ${list.length})`, 'color: #00ffff;', list);
        this.lastSystemsList = list;
      } else if (type === 0x09) { // MSG_SESSION custom forwarding
        const message = parseSessionPayload(payload);
        if (message) {
          console.log(`%c[Sim SDK IN] MSG_SESSION (0x09) forwarded message: ${message.kind} from="${message.from_name}" to="${message.to_name || '(all)'}"`, 'color: #a020f0; font-weight: bold;', message);
          this.sessionHandlers.forEach(handler => handler(message));
        } else {
          console.warn('[Sim SDK IN] MSG_SESSION payload could not be parsed as FabricSessionMessage!');
        }
      } else if (type === 0x86) { // MSG_CIRCUIT_DOWN
        console.log(`%c[Sim SDK IN] MSG_CIRCUIT_DOWN: simulated matrix circuit closed (arg: ${arg})`, 'color: #ff3333; font-weight: bold;');
        const err = new FabricUsbError('Simulated matrix circuit reset by peer or daemon');
        this.payloadWaiters.forEach(waiter => waiter.reject(err));
        this.payloadWaiters = [];
        this.receivingFileBuffer = null;
        this.receivingFileBytesReceived = 0;
        this.receivingFileTotalBytes = 0;
      }
    } else if (epId === 0x02) {
      // EP2 Data IN
      if (packet.length < 4) return;
      const view = new DataView(packet.buffer, packet.byteOffset, 4);
      const dataLen = view.getUint32(0);
      const rawPayload = packet.subarray(4, 4 + dataLen);

      console.log(`%c[Sim SDK IN] EP2 Data received (Length: ${dataLen} bytes)`, 'color: #ffaa00;');

      let parsedHeader: ParsedHeader | null = null;
      try {
        if (rawPayload.length >= 32) {
          parsedHeader = parseHeader(rawPayload.subarray(0, 32));
        }
      } catch {
        // Not a header chunk
      }

      if (parsedHeader && parsedHeader.fileSize > 0) {
        this.receivingFileTotalBytes = parsedHeader.fileSize;
        this.receivingFileBytesReceived = 0;
        this.receivingFileBuffer = new Uint8Array(parsedHeader.fileSize);
        this.receivingFilename = parsedHeader.filename;
        
        const fileData = rawPayload.subarray(32);
        this.receivingFileBuffer.set(fileData, 0);
        this.receivingFileBytesReceived += fileData.length;
      } else {
        if (this.receivingFileBuffer) {
          this.receivingFileBuffer.set(rawPayload, this.receivingFileBytesReceived);
          this.receivingFileBytesReceived += rawPayload.length;
        }
      }

      if (this.receivingFileBuffer && this.receivingFileBytesReceived >= this.receivingFileTotalBytes) {
        const payloadObj = { data: this.receivingFileBuffer, filename: this.receivingFilename };
        const total = this.receivingFileTotalBytes;
        this.receivingFileBuffer = null;
        this.receivingFileBytesReceived = 0;
        this.receivingFileTotalBytes = 0;

        if (this.onProgressCallback) {
          this.onProgressCallback(total, total);
        }

        if (this.payloadWaiters.length > 0) {
          const waiter = this.payloadWaiters.shift()!;
          waiter.resolve(payloadObj);
        } else {
          this.pendingPayloads.push(payloadObj);
        }
      } else {
        if (this.onProgressCallback && this.receivingFileBuffer) {
          this.onProgressCallback(this.receivingFileBytesReceived, this.receivingFileTotalBytes);
        }
      }
    }
  }

  async reconnectKnown(): Promise<string> {
    return this.connect();
  }

  describeDevice(): string {
    return `Sim · ${formatFabricPortDisplay(this.portIndex)}`;
  }

  async disconnect(): Promise<void> {
    if (this.socket) {
      const ws = this.socket;
      this.socket = null;

      // Reject all pending payload waiters immediately upon disconnect
      const err = new FabricUsbError('Simulated interface disconnected programmatically');
      this.payloadWaiters.forEach(waiter => waiter.reject(err));
      this.payloadWaiters = [];
      this.receivingFileBuffer = null;
      this.receivingFileBytesReceived = 0;
      this.receivingFileTotalBytes = 0;

      if (ws.readyState !== 3) { // 3 is CLOSED
        await new Promise<void>((resolve) => {
          const onClose = () => {
            ws.removeEventListener('close', onClose);
            ws.removeEventListener('error', onClose);
            resolve();
          };
          ws.addEventListener('close', onClose);
          ws.addEventListener('error', onClose);
          ws.close();
          // Safety timeout
          setTimeout(resolve, 300);
        });
      }
    }
  }

  async forgetThisDevice(): Promise<void> {
    await this.disconnect();
  }

  async resetConnection(): Promise<string> {
    await this.disconnect();
    return this.connect();
  }

  ownsDevice(_usbDevice: USBDevice): boolean {
    return false;
  }

  markDisconnected(): void {
    this.socket = null;
  }

  async sendBytes(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    if (!this.socket || this.socket.readyState !== 1) {
      throw new FabricUsbError('Sim not connected');
    }

    // 1. Establish bidirectional switching connection using real hardware Mode 11 Core spec:
    // Writes one 16-byte packet to EP4, low nibble of pkt[0] is dest_port (1-based index)
    const connectPacket = new Uint8Array(1 + 16);
    connectPacket[0] = 0x04; // EP4 Control OUT prefix
    connectPacket[1] = (this.targetPortIndex + 1) & 0x0F; // pkt[0] in hardware-level 16B envelope is byte 1 in prefix stream

    this.socket.send(connectPacket);

    // Give simulated crossbar matrix a moment to physically connect
    await new Promise((resolve) => setTimeout(resolve, 100));

    // Determine chunk size based on strategy (fallback from legacy 'all' is 256kb)
    let chunkSize = 262144;
    const strategy = this.getChunkSizeStrategy();
    if (strategy === '16kb') chunkSize = 16384;
    else if (strategy === '64kb') chunkSize = 65536;
    else if (strategy === '256kb') chunkSize = 262144;
    else if (strategy === '1mb') chunkSize = 1048576;

    // 2. Build the standard 32-byte ROCKETBX header
    const headerBuf = new Uint8Array(buildHeader(payload.length, {
      frameKind: 'payload',
      filename,
    }));

    // Send first packet with header + initial portion of data
    const firstChunkSize = Math.min(payload.length, chunkSize);
    const firstChunk = payload.subarray(0, firstChunkSize);

    const firstPayload = new Uint8Array(headerBuf.length + firstChunk.length);
    firstPayload.set(headerBuf, 0);
    firstPayload.set(firstChunk, headerBuf.length);

    // Prepend 0x01 EP1 Data OUT + 4-byte BE length prefix
    const firstPacket = new Uint8Array(1 + 4 + firstPayload.length);
    firstPacket[0] = 0x01; // EP1 Data OUT
    const view = new DataView(firstPacket.buffer);
    view.setUint32(1, firstPayload.length);
    firstPacket.set(firstPayload, 5);

    onProgress?.(0, payload.length);
    this.socket.send(firstPacket);
    let offset = firstChunkSize;
    onProgress?.(offset, payload.length);

    // Send remaining chunks
    while (offset < payload.length) {
      // Small sleep to let the event loop process, and prevent blocking the browser during giant file sends
      await new Promise((resolve) => setTimeout(resolve, 0));

      const nextChunkSize = Math.min(payload.length - offset, chunkSize);
      const nextChunk = payload.subarray(offset, offset + nextChunkSize);

      // Prepend 0x01 EP1 Data OUT + 4-byte BE length prefix
      const nextPacket = new Uint8Array(1 + 4 + nextChunk.length);
      nextPacket[0] = 0x01; // EP1 Data OUT
      const nextView = new DataView(nextPacket.buffer);
      nextView.setUint32(1, nextChunk.length);
      nextPacket.set(nextChunk, 5);

      this.socket.send(nextPacket);
      offset += nextChunkSize;
      onProgress?.(offset, payload.length);
    }

    // Give packets a moment to fully route over the switching plane before releasing circuit
    await new Promise((resolve) => setTimeout(resolve, 200));

    // 3. Disconnect bidirectional connection using real hardware Mode 11 Core spec (dest_port = 0)
    const disconnectPacket = new Uint8Array(1 + 16);
    disconnectPacket[0] = 0x04; // EP4 Control OUT prefix
    disconnectPacket[1] = 0 & 0x0F; // dest_port = 0 means disconnect

    this.socket.send(disconnectPacket);
  }

  async receiveHeader(): Promise<ParsedHeader> {
    throw new Error('Not implemented');
  }

  async receivePayload(
    _fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    const { data } = await this.receiveFileTransfer(15000, 0, onProgress);
    return data;
  }

  async discardPayload(fileSize: number): Promise<void> {
    await this.receivePayload(fileSize);
  }

  async receiveBytes(onProgress?: (done: number, total: number) => void): Promise<Uint8Array> {
    const { data } = await this.receiveFileTransfer(15000, 0, onProgress);
    return data;
  }

  private getChunkSizeStrategy(): string {
    if (typeof localStorage === 'undefined') {
      return '256kb';
    }
    const key = `rocketbox-identity-v1-port${this.portIndex}`;
    const raw = localStorage.getItem(key) || localStorage.getItem('rocketbox-identity-v1');
    if (!raw) {
      if (typeof window !== 'undefined') {
        const params = new URLSearchParams(window.location.search);
        return params.get('chunk_size') || '256kb';
      }
      return '256kb';
    }
    try {
      const parsed = JSON.parse(raw);
      return parsed.usb_read_buffer_size || '256kb';
    } catch {
      return '256kb';
    }
  }

  async sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    if (!this.socket || this.socket.readyState !== 1) {
      console.warn('[Sim SDK OUT] sendSessionMessage failed: not connected!', message);
      return;
    }
    const bytes = serializeSessionMessage(message);

    console.log(`%c[Sim SDK OUT] Sending Session Message: kind=${message.kind} from="${message.from_name}" to="${message.to_name || '(all)'}"`, 'color: #ff00ff; font-weight: bold;', message);

    if (message.kind === 'offer') {
      const toPortMatch = message.note?.match(/to_port=(\d+)/);
      if (toPortMatch) {
        this.targetPortIndex = parseInt(toPortMatch[1], 10);
      } else {
        // Fallback: If no to_port is present in the note, try to extract from target name / system ID
        const match = message.to_name?.match(/port[- ]?([1-4])/i);
        if (match) {
          this.targetPortIndex = parseInt(match[1], 10) - 1;
        } else {
          this.targetPortIndex = 0; // Default to port 1
        }
      }
    }

    const header = new Uint8Array(12);
    header[0] = 0x01; // ver
    header[1] = 0x09; // MSG_SESSION custom forwarding type
    const view = new DataView(header.buffer);
    view.setUint16(2, 99, false); // txn
    view.setUint32(4, this.portIndex, false); // arg (our port index)
    view.setUint32(8, bytes.length, false); // length

    const packet = new Uint8Array(1 + 12 + bytes.length);
    packet[0] = 0x04; // EP4 Control OUT
    packet.set(header, 1);
    packet.set(bytes, 13);
    this.socket.send(packet);
  }

  async tryReceiveSessionMessage(_headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    // Session messages are received on onmessage, so no polling needed
    return null;
  }

  async receiveFileTransfer(
    headerTimeoutMs: number,
    _expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    if (this.pendingPayloads.length > 0) {
      const payload = this.pendingPayloads.shift()!;
      onProgress?.(payload.data.length, payload.data.length);
      return payload;
    }

    this.onProgressCallback = onProgress || null;

    const timeoutMs = headerTimeoutMs > 0 ? headerTimeoutMs : 15000;

    return new Promise<{ data: Uint8Array; filename: string }>((resolve, reject) => {
      const waiter = { resolve, reject };
      this.payloadWaiters.push(waiter);

      const timer = setTimeout(() => {
        const idx = this.payloadWaiters.indexOf(waiter);
        if (idx !== -1) {
          this.payloadWaiters.splice(idx, 1);
          reject(new FabricUsbError('Transfer timed out - no payload received from peer'));
        }
      }, timeoutMs);

      // Wrap resolve/reject to clear the timer
      const originalResolve = resolve;
      const originalReject = reject;
      waiter.resolve = (val) => {
        clearTimeout(timer);
        this.onProgressCallback = null;
        originalResolve(val);
      };
      waiter.reject = (err) => {
        clearTimeout(timer);
        this.onProgressCallback = null;
        originalReject(err);
      };
    });
  }

  private handleClose(socketInstance: WebSocket, code?: number): void {
    if (this.socket !== socketInstance) {
      return;
    }
    this.socket = null;

    if (code === 4000) {
      console.log('[Sim SDK] Socket overridden by another client. Automatic reconnect disabled to allow takeover.');
      return;
    }

    console.log('[Sim SDK] Socket closed unexpectedly. Attempting automatic background reconnect in 1.5s...');
    this.attemptReconnect();
  }

  private attemptReconnect(): void {
    setTimeout(() => {
      if (this.socket !== null) {
        return;
      }
      this.connect().then(() => {
        console.log('[Sim SDK] Automatic background reconnect successful!');
      }).catch((err) => {
        console.log('[Sim SDK] Reconnect attempt failed:', err.message, 'Retrying in 1.5s...');
        this.attemptReconnect();
      });
    }, 1500);
  }
}
