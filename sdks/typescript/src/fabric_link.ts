import { boothLog } from './debug_log';
import {
  ALWAYS_LISTEN_HEADER_TIMEOUT_MS,
  HANDSHAKE_POLL_GAP_MS,
  LISTEN_POLL_GAP_MS,
  type ControlJob,
  type FabricLinkEvent,
  type ListenMode,
} from './link_types';
import { abortStuckTransfer, abortStuckTransferWithTimeout } from './link_bus';
import { enqueueControl } from './link_queue';
import { receiveFileTransfer, tryReceiveSessionMessage, tryReceiveSessionOnce } from './link_recv';
import { sendPayload, sendSessionMessage } from './link_send';
import type { FabricSessionMessage } from './session_types';
import { sleep } from './usb_util';

export type { FabricLinkEvent, ListenMode } from './link_types';
export { clearEndpointHalts } from './usb_claim';

export class FabricLink {
  usbTail: Promise<void> = Promise.resolve();
  controlQueue: ControlJob[] = [];
  controlDrainActive = false;
  interfaceRecovery: Promise<void> = Promise.resolve();
  activeInPoll: Promise<void> = Promise.resolve();
  pendingHeaderRead: Promise<USBInTransferResult | null> | null = null;
  listenSuspendedForSend = 0;
  listenMode: ListenMode = 'off';
  listenGeneration = 0;
  listenChainRunning = false;
  sessionSendTail: Promise<void> = Promise.resolve();
  readonly subscribers = new Set<(event: FabricLinkEvent) => void>();
  handshakePollTimeoutMs = 350;
  fabricLeg = 0;

  constructor(
    readonly getDevice: () => USBDevice | null,
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

  emit(event: FabricLinkEvent): void {
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
    this.listenChainRunning = false;
  }

  private async runListenChain(generation: number): Promise<void> {
    while (generation === this.listenGeneration && this.listenMode !== 'off') {
      const headerTimeout =
        this.listenMode === 'always'
          ? Math.min(ALWAYS_LISTEN_HEADER_TIMEOUT_MS, this.handshakePollTimeoutMs)
          : this.handshakePollTimeoutMs;
      const gapMs = this.listenMode === 'handshake' ? HANDSHAKE_POLL_GAP_MS : LISTEN_POLL_GAP_MS;
      try {
        await enqueueControl(
          this,
          'listenPoll',
          async () => {
            if (generation !== this.listenGeneration || this.listenMode === 'off') {
              return;
            }
            if (this.listenSuspendedForSend > 0) {
              return;
            }
            const poll = tryReceiveSessionOnce(this, headerTimeout);
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
        /* handled in tryReceiveSessionOnce */
      }
      if (generation !== this.listenGeneration) {
        return;
      }
      await sleep(gapMs);
    }
  }

  abortStuckTransfer(): Promise<void> {
    return abortStuckTransfer(this);
  }

  abortStuckTransferWithTimeout(): Promise<void> {
    return abortStuckTransferWithTimeout(this);
  }

  async prepareForPayloadSend(): Promise<void> {
    await this.waitForIdle();
    await this.abortStuckTransfer();
  }

  clearPendingHeaderRead(): void {
    this.pendingHeaderRead = null;
  }

  sendSessionMessage(message: FabricSessionMessage): Promise<void> {
    return sendSessionMessage(this, message);
  }

  sendPayload(
    payload: Uint8Array,
    onProgress?: (done: number, total: number) => void,
    filename = '',
  ): Promise<void> {
    return sendPayload(this, payload, onProgress, filename);
  }

  receiveFileTransfer(
    headerTimeoutMs: number,
    expectedBytes = 0,
    onProgress?: (done: number, total: number) => void,
  ): Promise<{ data: Uint8Array; filename: string }> {
    return receiveFileTransfer(this, headerTimeoutMs, expectedBytes, onProgress);
  }

  tryReceiveSessionMessage(headerTimeoutMs: number): Promise<FabricSessionMessage | null> {
    return tryReceiveSessionMessage(this, headerTimeoutMs);
  }
}
