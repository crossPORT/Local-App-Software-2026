import { FabricUsbError } from '../src/lib/fabric_errors';

const CHANNEL_NAME = 'rocketbox-fabric-sim-v2';

/** Simulated cable serials — leg = (parseInt(serial,16)-1) % 4 */
export const SIM_CABLE_SERIALS = [
  '0000000000000001',
  '0000000000000002',
  '0000000000000003',
  '0000000000000004',
] as const;

type HubMessage = {
  type: 'transmit';
  fromSerial: string;
  data: ArrayBuffer;
};

class SimReceiveBuffer {
  private chunks: Uint8Array[] = [];
  private totalBytes = 0;
  private wakeups: Array<() => void> = [];

  append(data: Uint8Array): void {
    if (data.length === 0) {
      return;
    }
    this.chunks.push(data);
    this.totalBytes += data.length;
    for (const wake of this.wakeups) {
      wake();
    }
    this.wakeups.length = 0;
  }

  clear(): void {
    this.chunks = [];
    this.totalBytes = 0;
    for (const wake of this.wakeups) {
      wake();
    }
    this.wakeups.length = 0;
  }

  private drain(count: number): Uint8Array {
    const out = new Uint8Array(count);
    let written = 0;
    while (written < count && this.chunks.length > 0) {
      const head = this.chunks[0]!;
      const take = Math.min(count - written, head.length);
      out.set(head.subarray(0, take), written);
      written += take;
      if (take === head.length) {
        this.chunks.shift();
      } else {
        this.chunks[0] = head.subarray(take);
      }
    }
    this.totalBytes -= count;
    return out;
  }

  private async waitForData(deadlineMs: number): Promise<void> {
    while (this.totalBytes === 0) {
      if (Date.now() >= deadlineMs) {
        throw new FabricUsbError('Header read failed');
      }
      await new Promise<void>((resolve) => {
        this.wakeups.push(resolve);
        window.setTimeout(resolve, Math.min(25, deadlineMs - Date.now()));
      });
    }
  }

  async readExact(count: number, timeoutMs: number): Promise<Uint8Array> {
    const deadlineMs = Date.now() + timeoutMs;
    while (this.totalBytes < count) {
      if (Date.now() >= deadlineMs) {
        throw new FabricUsbError('Header read failed');
      }
      await this.waitForData(deadlineMs);
    }
    return this.drain(count);
  }

  async readUpTo(maxBytes: number, timeoutMs: number): Promise<Uint8Array> {
    const deadlineMs = Date.now() + timeoutMs;
    await this.waitForData(deadlineMs);
    const take = Math.min(maxBytes, this.totalBytes);
    return this.drain(take);
  }
}

const rxBySerial = new Map<string, SimReceiveBuffer>();

function rxForSerial(serial: string): SimReceiveBuffer {
  let buffer = rxBySerial.get(serial);
  if (!buffer) {
    buffer = new SimReceiveBuffer();
    rxBySerial.set(serial, buffer);
  }
  return buffer;
}

let channel: BroadcastChannel | null = null;
let channelReady = false;

function ensureChannel(): BroadcastChannel | null {
  if (typeof BroadcastChannel === 'undefined') {
    return null;
  }
  if (!channel) {
    channel = new BroadcastChannel(CHANNEL_NAME);
  }
  if (!channelReady) {
    channelReady = true;
    channel.addEventListener('message', (event: MessageEvent<HubMessage>) => {
      const msg = event.data;
      if (msg?.type !== 'transmit' || !msg.fromSerial) {
        return;
      }
      for (const serial of SIM_CABLE_SERIALS) {
        if (serial !== msg.fromSerial) {
          rxForSerial(serial).append(new Uint8Array(msg.data));
        }
      }
    });
  }
  return channel;
}

export function fabricHubReset(): void {
  for (const buffer of rxBySerial.values()) {
    buffer.clear();
  }
}

export function fabricHubTransmit(fromSerial: string, data: Uint8Array): void {
  const bus = ensureChannel();
  if (!bus) {
    throw new FabricUsbError('Sim fabric requires BroadcastChannel (use two browser tabs)');
  }
  const copy = data.slice();
  bus.postMessage({ type: 'transmit', fromSerial, data: copy.buffer });
}

export function fabricHubReceiveBuffer(serial: string): SimReceiveBuffer {
  ensureChannel();
  return rxForSerial(serial);
}
