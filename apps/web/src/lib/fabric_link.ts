import { boothLog, getBoothLogLevel } from './booth_log';
import {
  CHUNK_SIZE,
  EP_IN,
  EP_OUT,
  HEADER_SIZE,
  INTERFACE_NUMBER,
  buildHeader,
  concatChunks,
  parseHeader,
} from './fabric_protocol';
import {
  type FabricSessionMessage,
  parseSessionPayload,
  serializeSessionMessage,
} from './fabric_session';
import { FabricUsbError } from './fabric_errors';
import { MAX_SESSION_FILE_BYTES } from './usb_constants';

const SESSION_SEND_TIMEOUT_MS = 2500;
const LISTEN_POLL_GAP_MS = 250;
const HANDSHAKE_POLL_GAP_MS = 50;
/** Half-duplex USB — let the previous IN poll fully retire before OUT (Android). */
const IN_SETTLE_MS = 120;
const IN_FLIGHT_DRAIN_MS = 600;
/** Never wait forever for a listen IN poll to retire before OUT. */
const ACTIVE_IN_POLL_WAIT_MS = 900;
const INTERFACE_RECOVERY_TIMEOUT_MS = 1500;
const SESSION_BODY_IN_TIMEOUT_MS = 800;
const ALWAYS_LISTEN_HEADER_TIMEOUT_MS = 200;
/**
 * Payload chunks stream on the no-buffer half-duplex fabric, so a chunk's
 * transferOut only retires once the receiver pulls it — far slower than a small
 * session frame. They must NOT inherit the aggressive session send timeout
 * (which would abort healthy large transfers); this is a generous safety net so
 * a genuinely dead peer cannot wedge the sender forever.
 */
const PAYLOAD_CHUNK_TIMEOUT_MS = 30000;

type Tier = 0 | 1;

function isUsbTransferContentionError(err: unknown): boolean {
  const message = (err as Error)?.message ?? '';
  return (
    message.includes('operation that changes the device state is in progress') ||
    message.includes('not part of a claimed') ||
    message.includes('A transfer error has occurred')
  );
}

/**
 * Clear bulk endpoint halts after claiming, mirroring the native libusb path
 * (`libusb_clear_halt` on EP_OUT/EP_IN). Without this, a release/claim cycle can
 * leave the OUT endpoint halted so every transferOut NAKs until it is cleared.
 */
export async function clearEndpointHalts(device: USBDevice): Promise<void> {
  if (typeof device.clearHalt !== 'function') {
    return;
  }
  try {
    await device.clearHalt('out', EP_OUT);
  } catch {
    /* best effort — endpoint may not be halted */
  }
  try {
    await device.clearHalt('in', EP_IN);
  } catch {
    /* best effort — endpoint may not be halted */
  }
}

async function ensureInterfaceReady(device: USBDevice): Promise<void> {
  if (!device.opened) {
    throw new FabricUsbError('USB not connected');
  }
  if (device.configuration == null) {
    await device.selectConfiguration(1);
  }
  for (let attempt = 0; attempt < 5; attempt += 1) {
    try {
      await device.claimInterface(INTERFACE_NUMBER);
      await clearEndpointHalts(device);
      return;
    } catch (err) {
      const message = (err as Error).message ?? '';
      if (message.includes('operation that changes the device state is in progress')) {
        await sleep(120 * (attempt + 1));
        continue;
      }
      if (
        message.includes('already claimed') ||
        message.includes('Unable to claim') ||
        message.includes('not part of a claimed')
      ) {
        try {
          await device.releaseInterface(INTERFACE_NUMBER);
        } catch {
          /* best effort */
        }
        if (device.configuration == null) {
          await device.selectConfiguration(1);
        }
        await sleep(80 * (attempt + 1));
        continue;
      }
      throw err;
    }
  }
  throw new FabricUsbError('USB interface claim failed after retries');
}

async function drainInFlightTransfer(
  inFlight: Promise<USBInTransferResult>,
): Promise<void> {
  await Promise.race([
    inFlight.catch(() => undefined),
    sleep(IN_FLIGHT_DRAIN_MS),
  ]);
  await sleep(IN_SETTLE_MS);
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

/** Idle half-duplex fabric — IN with no sender is normal, not a fault. */
function isBenignUsbListenError(err: unknown): boolean {
  const message = (err as Error)?.message ?? '';
  return (
    message === 'session header timeout' ||
    message.includes('transfer error') ||
    message.includes('transferIn returned no data') ||
    message.includes('transferIn failed')
  );
}

function mergeBuffers(parts: Uint8Array[], totalLength: number): Uint8Array {
  const out = new Uint8Array(totalLength);
  let offset = 0;
  for (const part of parts) {
    out.set(part, offset);
    offset += part.length;
  }
  return out;
}

async function transferInWithTimeout(
  device: USBDevice,
  byteCount: number,
  timeoutMs: number,
  cancelStuck?: () => Promise<void>,
): Promise<USBInTransferResult | null> {
  const inFlight = device.transferIn(EP_IN, byteCount);
  // A WebUSB transferIn has no native timeout: when we give up on the JS side
  // it is still outstanding on the IN endpoint. If it is not cancelled it stays
  // queued and grabs the next frame the hub delivers — and because nothing is
  // awaiting it any more, that frame is silently discarded. Left to accumulate,
  // these orphaned reads steal every inbound frame and kill all receives with
  // no error logged. Swallow the late settle so cancelling never surfaces as an
  // unhandled rejection.
  inFlight.catch(() => undefined);
  let timeoutId: number | undefined;
  try {
    const raced = await Promise.race([
      inFlight,
      new Promise<'timeout'>((resolve) => {
        timeoutId = window.setTimeout(() => resolve('timeout'), timeoutMs);
      }),
    ]);
    if (timeoutId !== undefined) {
      window.clearTimeout(timeoutId);
    }
    if (raced === 'timeout') {
      // Cancel the still-outstanding read (release/reclaim aborts it) so it can
      // never steal a later frame. Falls back to a passive drain only when no
      // canceller is supplied.
      if (cancelStuck) {
        await cancelStuck();
      } else {
        await drainInFlightTransfer(inFlight);
      }
      return null;
    }
    return raced;
  } catch (err) {
    if (timeoutId !== undefined) {
      window.clearTimeout(timeoutId);
    }
    // The transfer already retired with an error — there is no orphan to cancel.
    await drainInFlightTransfer(inFlight);
    if (isBenignUsbListenError(err)) {
      return null;
    }
    throw err;
  }
}

async function transferInExact(device: USBDevice, byteCount: number): Promise<Uint8Array> {
  const parts: Uint8Array[] = [];
  let received = 0;
  while (received < byteCount) {
    const result = await device.transferIn(EP_IN, byteCount - received);
    if (result.status !== 'ok') {
      throw new FabricUsbError(`transferIn failed: ${result.status}`);
    }
    const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
    if (chunk.length === 0) {
      throw new FabricUsbError('transferIn returned no data');
    }
    parts.push(chunk);
    received += chunk.length;
  }
  return mergeBuffers(parts, byteCount);
}

async function transferOutChecked(device: USBDevice, data: BufferSource): Promise<void> {
  const result = await device.transferOut(EP_OUT, data);
  if (result.status !== 'ok') {
    throw new FabricUsbError(`transferOut failed: ${result.status}`);
  }
}

async function transferOutWithTimeout(
  device: USBDevice,
  data: BufferSource,
  timeoutMs: number,
): Promise<void> {
  const inFlight = transferOutChecked(device, data);
  // A wedged WebUSB transferOut may never settle. Never block on it: attach a
  // no-op catch so its eventual rejection (e.g. after the interface is
  // released during recovery) does not surface as an unhandled rejection.
  inFlight.catch(() => undefined);
  let timeoutId: number | undefined;
  const timeout = new Promise<'timeout'>((resolve) => {
    timeoutId = window.setTimeout(() => resolve('timeout'), timeoutMs);
  });
  try {
    const raced = await Promise.race([inFlight.then(() => 'ok' as const), timeout]);
    if (raced === 'timeout') {
      throw new FabricUsbError(`transferOut timed out after ${timeoutMs}ms`);
    }
  } finally {
    if (timeoutId !== undefined) {
      window.clearTimeout(timeoutId);
    }
  }
}

async function transferOutWithRetry(
  device: USBDevice,
  data: BufferSource,
  timeoutMs: number,
  recover: () => Promise<void>,
): Promise<void> {
  for (let attempt = 0; attempt < 2; attempt += 1) {
    try {
      await transferOutWithTimeout(device, data, timeoutMs);
      return;
    } catch (err) {
      // Recovery releases + reclaims the interface, which cancels any wedged
      // transfer so the bus (and listen) can resume promptly.
      await recover();
      const contention = isUsbTransferContentionError(err);
      // A plain timeout means the endpoint is wedged — retrying the same wedged
      // endpoint won't help, so bail fast and let listening resume. Only retry
      // transient contention (concurrent claim/release races).
      if (!contention || attempt === 1) {
        throw err;
      }
      await sleep(60);
    }
  }
}

export type ListenMode = 'always' | 'handshake' | 'off';

export type FabricLinkEvent =
  | { type: 'session'; message: FabricSessionMessage }
  | { type: 'error'; error: Error };

/**
 * Traffic controller for half-duplex WebUSB: Tier 0 control before Tier 1 payload chunks.
 */
type ControlJob = {
  label: string;
  priority: number;
  run: () => Promise<void>;
  resolve: () => void;
  reject: (err: unknown) => void;
};

export class FabricLink {
  private usbTail: Promise<void> = Promise.resolve();
  private controlQueue: ControlJob[] = [];
  private controlDrainActive = false;
  private interfaceRecovery: Promise<void> = Promise.resolve();
  private activeInPoll: Promise<void> = Promise.resolve();
  /**
   * The single outstanding background header read. A WebUSB bulk-IN has no
   * native timeout — it parks until the peer transmits — so we keep ONE read
   * alive across listen polls instead of discarding it on every soft timeout.
   */
  private pendingHeaderRead: Promise<USBInTransferResult | null> | null = null;
  private listenSuspendedForSend = 0;
  private listenMode: ListenMode = 'off';
  private listenGeneration = 0;
  private listenChainRunning = false;
  private sessionSendTail: Promise<void> = Promise.resolve();
  private readonly subscribers = new Set<(event: FabricLinkEvent) => void>();
  private handshakePollTimeoutMs = 350;
  private fabricLeg = 0;

  constructor(
    private getDevice: () => USBDevice | null,
    fabricLeg = 0,
  ) {
    this.fabricLeg = fabricLeg;
  }

  setFabricLeg(leg: number): void {
    this.fabricLeg = leg;
  }

  setHandshakePollTimeoutMs(ms: number): void {
    this.handshakePollTimeoutMs = ms;
  }

  setListenMode(mode: ListenMode): void {
    const prev = this.listenMode;
    if (mode === this.listenMode) {
      if (mode !== 'off' && !this.listenChainRunning) {
        this.startListenLoop();
      }
      return;
    }
    this.listenMode = mode;
    boothLog(this.fabricLeg, 'listen_mode', mode);
    if (mode === 'off') {
      this.stopListenLoop();
    } else if (prev === 'off' || !this.listenChainRunning) {
      this.startListenLoop();
    }
  }

  getListenMode(): ListenMode {
    return this.listenMode;
  }

  /** Restart background listen if the chain died (e.g. after outbound prep). */
  ensureListening(): void {
    if (this.listenMode !== 'off' && !this.listenChainRunning) {
      boothLog(this.fabricLeg, 'listen_restart', 'watchdog');
      this.startListenLoop();
    }
  }

  subscribe(cb: (event: FabricLinkEvent) => void): () => void {
    this.subscribers.add(cb);
    return () => this.subscribers.delete(cb);
  }

  private emit(event: FabricLinkEvent): void {
    for (const cb of this.subscribers) {
      cb(event);
    }
  }

  async waitForIdle(): Promise<void> {
    await this.usbTail;
  }

  startListenLoop(): void {
    this.stopListenLoop();
    const generation = ++this.listenGeneration;
    this.listenChainRunning = true;
    void this.runListenChain(generation).finally(() => {
      if (this.listenGeneration === generation) {
        this.listenChainRunning = false;
      }
    });
  }

  stopListenLoop(): void {
    this.listenGeneration += 1;
    // Generation bump alone does not clear this — the chain .finally only runs
    // when generations still match, so outbound prep would leave listen dead.
    this.listenChainRunning = false;
  }

  private async runListenChain(generation: number): Promise<void> {
    while (generation === this.listenGeneration && this.listenMode !== 'off') {
      const headerTimeout =
        this.listenMode === 'always'
          ? Math.min(ALWAYS_LISTEN_HEADER_TIMEOUT_MS, this.handshakePollTimeoutMs)
          : this.handshakePollTimeoutMs;
      const gapMs =
        this.listenMode === 'handshake' ? HANDSHAKE_POLL_GAP_MS : LISTEN_POLL_GAP_MS;

      try {
        await this.enqueueControl(
          'listenPoll',
          async () => {
            if (generation !== this.listenGeneration || this.listenMode === 'off') {
              return;
            }
            if (this.listenSuspendedForSend > 0) {
              return;
            }
            const poll = this.tryReceiveSessionOnce(headerTimeout);
            this.activeInPoll = poll.then(
              () => undefined,
              () => undefined,
            );
            const message = await poll;
            if (message) {
              boothLog(this.fabricLeg, 'session_frame', message.kind);
              this.emit({ type: 'session', message });
            }
          },
          'listen',
        );
      } catch {
        /* listen poll errors are handled inside tryReceiveSessionOnce */
      }

      if (generation !== this.listenGeneration) {
        return;
      }
      await sleep(gapMs);
    }
  }

  /**
   * Synchronous, non-throwing half of outbound prep: suspend the listen loop.
   * MUST be paired with `endOutboundTransfer` in a `finally`. Kept separate from
   * the throwable `prepareOutboundBus` so a failure during bus prep (e.g. an
   * interface claim error in a reconnect window) can never leak the suspend
   * counter — a leak would make every listen poll early-return and silently kill
   * all inbound receives until the next reconnect.
   */
  private beginOutboundTransfer(): void {
    this.listenSuspendedForSend += 1;
    this.stopListenLoop();
  }

  private async prepareOutboundBus(): Promise<void> {
    boothLog(this.fabricLeg, 'session_send_prep', 'wait_in');
    await Promise.race([this.activeInPoll, sleep(ACTIVE_IN_POLL_WAIT_MS)]);
    boothLog(this.fabricLeg, 'session_send_prep', 'abort_iface');
    await this.abortStuckTransferWithTimeout();
    await sleep(IN_SETTLE_MS);
    boothLog(this.fabricLeg, 'session_send_prep', 'ready');
  }

  private endOutboundTransfer(): void {
    this.listenSuspendedForSend = Math.max(0, this.listenSuspendedForSend - 1);
    if (this.listenMode !== 'off') {
      boothLog(this.fabricLeg, 'listen_restart', this.listenMode);
      this.startListenLoop();
    }
  }

  async abortStuckTransfer(): Promise<void> {
    const device = this.getDevice();
    if (!device?.opened) {
      return;
    }
    // Chain recoveries so they never overlap, but keep the stored tail
    // unpoisoned: if this recovery rejects, the next call must still run rather
    // than inherit a permanently-rejected promise.
    const recovery = this.interfaceRecovery.then(
      () => this.runInterfaceRecovery(),
      () => this.runInterfaceRecovery(),
    );
    this.interfaceRecovery = recovery.then(
      () => undefined,
      () => undefined,
    );
    await recovery;
  }

  private async runInterfaceRecovery(): Promise<void> {
    const current = this.getDevice();
    if (!current?.opened) {
      return;
    }
    // Releasing the interface cancels the outstanding header read, so drop our
    // reference to it — the next listen poll must arm a fresh read.
    this.clearPendingHeaderRead();
    try {
      await current.releaseInterface(INTERFACE_NUMBER);
    } catch {
      /* best effort */
    }
    await ensureInterfaceReady(current);
  }

  private async abortStuckTransferWithTimeout(): Promise<void> {
    await Promise.race([
      this.abortStuckTransfer(),
      sleep(INTERFACE_RECOVERY_TIMEOUT_MS),
    ]);
  }

  async prepareForPayloadSend(): Promise<void> {
    await this.waitForIdle();
    await this.abortStuckTransfer();
  }

  private runUsb<T>(tier: Tier, label: string, fn: () => Promise<T>): Promise<T> {
    if (getBoothLogLevel() === 'verbose') {
      boothLog(this.fabricLeg, 'usb_queue', `tier=${tier} ${label}`);
    }
    const run = this.usbTail.then(() => fn());
    this.usbTail = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }

  private enqueueControl(
    label: string,
    fn: () => Promise<void>,
    priority: 'listen' | 'session' = 'session',
  ): Promise<void> {
    return new Promise((resolve, reject) => {
      const job: ControlJob = {
        label,
        priority: priority === 'listen' ? 0 : 1,
        run: fn,
        resolve: () => resolve(),
        reject,
      };
      if (priority === 'session') {
        this.controlQueue.unshift(job);
      } else if (this.controlQueue.some((entry) => entry.priority === 0)) {
        job.resolve();
        return;
      } else {
        this.controlQueue.push(job);
      }
      void this.drainControlQueue();
    });
  }

  private async drainControlQueue(): Promise<void> {
    if (this.controlDrainActive) {
      return;
    }
    this.controlDrainActive = true;
    try {
      while (this.controlQueue.length > 0) {
        const job = this.controlQueue.shift()!;
        try {
          await this.runUsb(0, job.label, job.run);
          job.resolve();
        } catch (err) {
          job.reject(err);
        }
      }
    } finally {
      this.controlDrainActive = false;
      if (this.controlQueue.length > 0) {
        void this.drainControlQueue();
      }
    }
  }

  async sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    const bytes = serializeSessionMessage(message);
    if (bytes.length > MAX_SESSION_FILE_BYTES) {
      throw new FabricUsbError('Session message too large');
    }
    // Serialize concurrent sends so they never overlap on the half-duplex bus.
    const run = this.sessionSendTail.then(
      () => this.runSessionSend(message, bytes),
      () => this.runSessionSend(message, bytes),
    );
    this.sessionSendTail = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }

  private async runSessionSend(
    message: FabricSessionMessage,
    bytes: Uint8Array,
  ): Promise<void> {
    await this.enqueueControl('sendSession', async () => {
      const device = this.getDevice();
      if (!device) {
        throw new FabricUsbError('USB not connected');
      }
      const recover = () => this.abortStuckTransferWithTimeout();
      this.beginOutboundTransfer();
      try {
        await this.prepareOutboundBus();
        await transferOutWithRetry(
          device,
          buildHeader(bytes.length, { frameKind: 'session' }),
          SESSION_SEND_TIMEOUT_MS,
          recover,
        );
        await transferOutWithRetry(device, new Uint8Array(bytes), SESSION_SEND_TIMEOUT_MS, recover);
        boothLog(this.fabricLeg, 'session_sent', message.kind);
      } finally {
        this.endOutboundTransfer();
      }
    });
  }

  async sendPayload(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    await this.runUsb(1, 'sendPayload', async () => {
      const device = this.getDevice();
      if (!device) {
        throw new FabricUsbError('USB not connected');
      }
      this.beginOutboundTransfer();
      try {
        // Prep the half-duplex bus ONCE, then stream every chunk back-to-back.
        // The fabric has no buffering, so a chunk's transferOut only retires once
        // the receiver pulls it — it can take far longer than a small session
        // frame. Use a generous payload timeout and NEVER release/claim the
        // interface between chunks: a mid-stream recovery (or the aggressive 2.5s
        // session timeout) would desync the receiver's chunk reader and abort an
        // otherwise-healthy transfer, which is what stalled large sends.
        await this.prepareOutboundBus();
        await transferOutWithTimeout(
          device,
          buildHeader(payload.length, { frameKind: 'payload', filename }),
          PAYLOAD_CHUNK_TIMEOUT_MS,
        );
        onProgress?.(0, payload.length);

        let offset = 0;
        while (offset < payload.length) {
          const chunk = new Uint8Array(CHUNK_SIZE);
          const n = Math.min(payload.length - offset, CHUNK_SIZE);
          chunk.set(payload.subarray(offset, offset + n));
          await transferOutWithTimeout(device, chunk, PAYLOAD_CHUNK_TIMEOUT_MS);
          offset += n;
          onProgress?.(offset, payload.length);
        }
        boothLog(this.fabricLeg, 'payload_sent', String(payload.length));
      } finally {
        this.endOutboundTransfer();
      }
    });
  }

  /**
   * Issue (or reuse) the single outstanding background header read. A WebUSB
   * bulk-IN has no native timeout — it parks until the peer transmits — and the
   * fabric does NOT buffer, so a frame only lands if a read is OUTSTANDING at the
   * instant the peer sends it. We therefore keep ONE header read alive across
   * listen polls instead of cancelling it on every soft timeout: a poll that
   * times out leaves this read in flight, so the IN endpoint stays armed ~100%
   * of the time and the next poll picks up the frame the moment it arrives.
   * Cancelling on every timeout (release/claim) left long armed-less gaps that
   * dropped most cross-leg announces, so discovery took minutes.
   */
  private headerRead(device: USBDevice): Promise<USBInTransferResult | null> {
    if (this.pendingHeaderRead) {
      return this.pendingHeaderRead;
    }
    const read = device.transferIn(EP_IN, HEADER_SIZE).then(
      (result) => result as USBInTransferResult | null,
      (err) => {
        if (!isBenignUsbListenError(err) && (err as Error)?.name !== 'AbortError') {
          boothLog(this.fabricLeg, 'usb_recv_fail', (err as Error).message);
        }
        return null;
      },
    );
    this.pendingHeaderRead = read;
    return read;
  }

  /** Drop the reference to the outstanding header read (consumed or cancelled). */
  private clearPendingHeaderRead(): void {
    this.pendingHeaderRead = null;
  }

  private async tryReceiveSessionOnce(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    const device = this.getDevice();
    if (!device) {
      return null;
    }

    try {
      const headerRead = this.headerRead(device);
      const raced = await Promise.race([
        headerRead.then((result) => ({ ready: true as const, result })),
        sleep(headerTimeoutMs).then(() => ({ ready: false as const, result: null })),
      ]);
      if (!raced.ready) {
        // Soft timeout: yield the bus so a queued send can run, but leave the
        // header read OUTSTANDING so the next poll still catches this frame.
        return null;
      }
      this.clearPendingHeaderRead();
      const result = raced.result;
      if (!result || result.status !== 'ok' || !result.data?.byteLength) {
        return null;
      }
      const chunk = new Uint8Array(
        result.data.buffer,
        result.data.byteOffset,
        result.data.byteLength,
      );
      if (chunk.length < HEADER_SIZE) {
        return null;
      }

      const header = parseHeader(chunk.subarray(0, HEADER_SIZE));
      if (header.frameKind !== 'session') {
        boothLog(this.fabricLeg, 'frame_kind_reject', `expected session got ${header.frameKind}`);
        if (header.fileSize > 0) {
          await this.receivePayloadBytes(header.fileSize).catch(() => {});
        }
        return null;
      }
      if (header.fileSize === 0 || header.fileSize > MAX_SESSION_FILE_BYTES) {
        if (header.fileSize > MAX_SESSION_FILE_BYTES) {
          await this.receivePayloadBytes(header.fileSize).catch(() => {});
        }
        return null;
      }
      const data = await this.receivePayloadBytesWithTimeout(header.fileSize, SESSION_BODY_IN_TIMEOUT_MS);
      if (!data) {
        return null;
      }
      return parseSessionPayload(data);
    } catch (err) {
      this.clearPendingHeaderRead();
      if (isBenignUsbListenError(err)) {
        return null;
      }
      boothLog(this.fabricLeg, 'usb_recv_fail', (err as Error).message);
      return null;
    }
  }

  private async receivePayloadBytesWithTimeout(
    fileSize: number,
    chunkTimeoutMs: number,
  ): Promise<Uint8Array | null> {
    const device = this.getDevice();
    if (!device) {
      return null;
    }
    const parts: Uint8Array[] = [];
    let received = 0;
    while (received < fileSize) {
      const result = await transferInWithTimeout(device, CHUNK_SIZE, chunkTimeoutMs, () =>
        this.abortStuckTransfer(),
      );
      if (!result || result.status !== 'ok') {
        boothLog(this.fabricLeg, 'session_body_timeout', `${received}/${fileSize}`);
        return null;
      }
      const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
      if (chunk.length === 0) {
        return null;
      }
      const take = Math.min(chunk.length, fileSize - received);
      parts.push(chunk.subarray(0, take));
      received += take;
    }
    return concatChunks(parts, fileSize);
  }

  private async receivePayloadBytes(
    fileSize: number,
    onProgress?: (done: number, total: number) => void,
  ): Promise<Uint8Array> {
    const device = this.getDevice();
    if (!device) {
      throw new FabricUsbError('USB not connected');
    }
    const parts: Uint8Array[] = [];
    let received = 0;
    while (received < fileSize) {
      const result = await device.transferIn(EP_IN, CHUNK_SIZE);
      if (result.status !== 'ok') {
        throw new FabricUsbError(`Payload transferIn failed: ${result.status}`);
      }
      const chunk = new Uint8Array(result.data!.buffer, result.data!.byteOffset, result.data!.byteLength);
      const take = Math.min(chunk.length, fileSize - received);
      parts.push(chunk.subarray(0, take));
      received += take;
      onProgress?.(received, fileSize);
    }
    return concatChunks(parts, fileSize);
  }

  async receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    return this.runUsb(1, 'receiveFileTransfer', async () => {
      const device = this.getDevice();
      if (!device) {
        throw new FabricUsbError('USB not connected');
      }

      const deadline = Date.now() + headerTimeoutMs;
      let skipped = 0;
      const maxSkips = 16;

      while (Date.now() < deadline && skipped < maxSkips) {
        const remaining = Math.max(1, deadline - Date.now());
        let timeoutId: number | undefined;
        let timedOut = false;
        try {
          const headerBuf = await Promise.race([
            transferInExact(device, HEADER_SIZE),
            new Promise<never>((_, reject) => {
              timeoutId = window.setTimeout(() => {
                timedOut = true;
                reject(new FabricUsbError('payload header timeout'));
              }, remaining);
            }),
          ]);
          if (timeoutId !== undefined) {
            window.clearTimeout(timeoutId);
          }

          const header = parseHeader(headerBuf);
          if (header.frameKind === 'session') {
            const data = await this.receivePayloadBytes(header.fileSize);
            const parsed = parseSessionPayload(data);
            if (parsed) {
              boothLog(this.fabricLeg, 'stray_session_skipped', parsed.kind);
              this.emit({ type: 'session', message: parsed });
            }
            skipped += 1;
            continue;
          }
          if (header.frameKind !== 'payload') {
            throw new FabricUsbError('Invalid frame kind during payload receive');
          }
          if (header.fileSize === 0) {
            skipped += 1;
            continue;
          }
          if (expectedBytes > 0 && header.fileSize !== expectedBytes) {
            throw new FabricUsbError(
              `Expected ${expectedBytes} byte payload but header announced ${header.fileSize}`,
            );
          }

          const trackProgress = expectedBytes > 0 && header.fileSize === expectedBytes;
          const payload = await this.receivePayloadBytes(
            header.fileSize,
            trackProgress ? onProgress : undefined,
          );
          return {
            data: payload,
            filename: header.filename || 'download.bin',
          };
        } catch (err) {
          if (timeoutId !== undefined) {
            window.clearTimeout(timeoutId);
          }
          if (timedOut) {
            await this.abortStuckTransfer();
          }
          throw err;
        }
      }

      throw new FabricUsbError('payload header timeout');
    });
  }

  /** Legacy poll API — delegates to one listen receive. */
  async tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    return this.runUsb(0, 'tryReceiveSession', () => this.tryReceiveSessionOnce(headerTimeoutMs));
  }
}
