import { rollBoothDisplayMibS } from './fabric_protocol';
import { formatBytes, formatTransferDoneMessage, isTransferCompleteMessage } from './format';
import { parseAnnounceNote } from './announce_note';
import {
  buildAnnounceMessage,
  buildSessionReply,
  type FabricSessionMessage,
  makeSessionId,
} from './fabric_session';
import type { FabricTransport } from './fabric_transport';
import { PeerRoster } from './peer_roster';
import { readFilePayload } from './fabric_usb';
import type { AppUiState, IdentityProfile, PendingOffer, ReceiveStatus } from './types';
import { formatTransferError } from './user_errors';
import { handshakeTimingFromIdentity } from './session_handshake';
import type { HandshakeTiming } from './session_handshake';
import { TRANSFER_DONE_DISMISS_MS } from './usb_constants';

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

function effectiveReceiveStatus(status: ReceiveStatus): ReceiveStatus {
  return status === 'busy' ? 'open' : status;
}

function toPendingOffer(message: FabricSessionMessage): PendingOffer {
  return {
    from_name: message.from_name,
    team: message.team,
    payload_name: message.payload_name,
    total_bytes: message.total_bytes,
    file_count: message.file_count || 1,
    note: message.note,
    session_id: message.session_id,
  };
}

export const ANNOUNCE_INTERVAL_MS = 10_000;
const ANNOUNCE_JITTER_MS = 1_000;

export interface OrchestratorCallbacks {
  patch: (partial: Partial<AppUiState>) => void;
  getIdentity: () => IdentityProfile;
  getPortIndex: () => number;
  getRoster: () => PeerRoster;
  onUsbDescription: (desc: string) => void;
  downloadPayload: (data: Uint8Array, filename: string) => void;
  isDisconnecting: () => boolean;
}

export class WebTransferOrchestrator {
  private listenerCancel = false;
  private listenerPaused = false;
  private listenerRunning = false;
  private lastAnnounceMs = 0;

  private busy = false;
  private outboundOffer: FabricSessionMessage | null = null;
  private awaitingReady = false;
  /** Sender must not hold USB IN until the receiver has had time to read the offer. */
  private offerReceiveGraceUntil = 0;
  /** After ready, stop session IN polls so payload OUT is not blocked on runUsb. */
  private payloadSendPending = false;

  private pendingInbound: FabricSessionMessage | null = null;

  private acceptWaiter: { resolve: () => void; reject: (err: Error) => void } | null = null;
  private readyWaiter: { resolve: () => void; reject: (err: Error) => void } | null = null;
  private dismissTimer: ReturnType<typeof setTimeout> | null = null;
  private dismissEpoch = 0;
  private offerRetransmitSent = false;
  private fabricActivitySeq = 0;

  constructor(
    private readonly usb: FabricTransport,
    private readonly callbacks: OrchestratorCallbacks,
  ) {}

  get isBusy(): boolean {
    return this.busy;
  }

  get hasPendingOffer(): boolean {
    return this.pendingInbound != null;
  }

  startListener(): void {
    if (this.listenerRunning) {
      return;
    }
    this.listenerCancel = false;
    this.lastAnnounceMs = 0;
    this.listenerRunning = true;
    void this.listenerLoop();
  }

  stopListener(): void {
    this.listenerCancel = true;
    this.listenerPaused = false;
  }

  sendAnnounceNow(): void {
    void this.maybeSendAnnounce(true);
  }

  private publishRoster(): void {
    this.callbacks.patch({
      roster: this.callbacks.getRoster().visiblePeers(this.usb.connected),
    });
  }

  clearTransferState(): void {
    this.cancelDismissTimer();
    this.busy = false;
    this.pendingInbound = null;
    this.outboundOffer = null;
    this.awaitingReady = false;
    this.offerReceiveGraceUntil = 0;
    this.payloadSendPending = false;
    this.rejectWaiters(new Error('Transfer cleared'));
    this.resumeListener();
  }

  private rejectWaiters(err: Error): void {
    this.acceptWaiter?.reject(err);
    this.readyWaiter?.reject(err);
    this.acceptWaiter = null;
    this.readyWaiter = null;
  }

  private pauseListener(): void {
    this.listenerPaused = true;
  }

  private resumeListener(): void {
    this.listenerPaused = false;
    this.offerReceiveGraceUntil = 0;
  }

  private cancelDismissTimer(): void {
    this.dismissEpoch += 1;
    if (this.dismissTimer !== null) {
      window.clearTimeout(this.dismissTimer);
      this.dismissTimer = null;
    }
  }

  private scheduleDismissTransfer(): void {
    this.cancelDismissTimer();
    const dismissEpoch = ++this.dismissEpoch;
    this.dismissTimer = window.setTimeout(() => {
      this.dismissTimer = null;
      if (dismissEpoch !== this.dismissEpoch || this.busy) {
        return;
      }
      this.callbacks.patch({
        statusMessage: '',
        notification: '',
        bytesDone: 0,
        bytesTotal: 0,
        transferLabel: '',
        liveMbps: 0,
        peakMbps: 0,
        resultMbps: 0,
        boothDisplayMibS: 0,
        fabricActivityMbps: 0,
      });
    }, TRANSFER_DONE_DISMISS_MS);
  }

  private finishTransfer(
    ok: boolean,
    message: string,
    error = '',
    speeds?: { peak: number; result: number },
  ): void {
    this.busy = false;
    this.callbacks.patch({
      busy: false,
      waitingForPartner: false,
      pendingOffer: null,
      statusMessage: ok ? message : '',
      errorMessage: ok ? '' : error,
      liveMbps: 0,
      fabricActivityMbps: 0,
      notification: ok ? message : '',
      peakMbps: ok ? (speeds?.peak ?? 0) : 0,
      resultMbps: ok ? (speeds?.result ?? 0) : 0,
      boothDisplayMibS: 0,
    });
    if (ok && isTransferCompleteMessage(message)) {
      this.scheduleDismissTransfer();
    }
  }

  private boothDisplayRate(identity: IdentityProfile): number {
    const base = identity.booth_display_mib_s;
    return base > 0 ? rollBoothDisplayMibS(base, identity.booth_display_jitter_pct) : 0;
  }

  private bumpSessionActivity(): void {
    this.fabricActivitySeq += 1;
    this.callbacks.patch({ fabricActivitySeq: this.fabricActivitySeq });
  }

  private patchTransferProgress(partial: Partial<AppUiState>): void {
    const live = partial.liveMbps ?? 0;
    if (live > 0) {
      this.callbacks.patch({ ...partial, fabricActivityMbps: live });
      return;
    }
    this.callbacks.patch(partial);
  }

  private async maybeSendAnnounce(force: boolean): Promise<void> {
    if (this.listenerCancel || !this.usb.connected || this.busy || this.listenerPaused) {
      return;
    }
    const identity = this.callbacks.getIdentity();
    if (!identity.display_name.trim()) {
      return;
    }
    const now = Date.now();
    if (!force && this.lastAnnounceMs > 0 && now - this.lastAnnounceMs < ANNOUNCE_INTERVAL_MS) {
      return;
    }
    try {
      const message = buildAnnounceMessage(identity, this.callbacks.getPortIndex());
      await this.usb.sendSessionMessage(message);
      this.lastAnnounceMs = now;
      this.bumpSessionActivity();
      this.callbacks.patch({ lastAnnounceMs: now });
      console.log(`[RocketBox] announce SENT as "${identity.display_name}"`);
    } catch (err) {
      console.warn('[RocketBox] announce send failed:', (err as Error).message);
    }
  }

  private handshake(): HandshakeTiming {
    return handshakeTimingFromIdentity(this.callbacks.getIdentity());
  }

  private async listenerLoop(): Promise<void> {
    console.log('[RocketBox] listenerLoop: starting');
    await sleep(300 + Math.floor(Math.random() * ANNOUNCE_JITTER_MS));

    console.log('[RocketBox] listenerLoop: initial burst');
    for (let i = 0; i < 3 && !this.listenerCancel && this.usb.connected; i++) {
      console.debug(`[RocketBox] listenerLoop: burst announce ${i + 1}/3`);
      await this.maybeSendAnnounce(true);
      await sleep(500);
    }
    console.log('[RocketBox] listenerLoop: entering main loop');

    while (!this.listenerCancel && this.usb.connected) {
      if (
        this.listenerPaused ||
        this.payloadSendPending ||
        (this.busy && !this.outboundOffer) ||
        this.pendingInbound != null ||
        this.callbacks.isDisconnecting()
      ) {
        await sleep(200);
        continue;
      }

      if (
        this.outboundOffer &&
        !this.awaitingReady &&
        Date.now() < this.offerReceiveGraceUntil
      ) {
        await sleep(10);
        continue;
      }

      const handshakePoll = this.outboundOffer != null;
      const headerTimeout = handshakePoll
        ? this.handshake().handshake_poll_timeout_ms
        : this.handshake().session_header_timeout_ms;

      // Poll IN before OUT — fabric does not buffer; gaps between reads drop accept/ready.
      const message = await this.usb.tryReceiveSessionMessage(headerTimeout);
      if (message && !this.listenerCancel) {
        this.onSessionMessage(message);
      }

      if (!this.outboundOffer) {
        await this.maybeSendAnnounce(false);
      }

      if (!this.listenerCancel && !this.outboundOffer) {
        await sleep(50);
      }
    }
    this.listenerRunning = false;
  }

  private onSessionMessage(message: FabricSessionMessage): void {
    console.log(`[RocketBox] session message received: kind=${message.kind} from="${message.from_name}"`);
    this.bumpSessionActivity();
    switch (message.kind) {
      case 'offer':
        void this.handleOffer(message);
        break;
      case 'accept':
        this.handleAccept(message);
        break;
      case 'decline':
        this.handleDecline(message);
        break;
      case 'ready':
        this.handleReady(message);
        break;
      case 'announce':
        this.handleAnnounce(message);
        break;
      default:
        break;
    }
  }

  private handleAnnounce(message: FabricSessionMessage): void {
    const identity = this.callbacks.getIdentity();
    if (!message.from_name || message.from_name === identity.display_name) {
      return;
    }
    console.log(`[RocketBox] announce RECEIVED from "${message.from_name}"`);
    const defaultPort = this.callbacks.getPortIndex() === 0 ? 1 : 0;
    const { portIndex, receiveStatus } = parseAnnounceNote(message.note, defaultPort);
    this.callbacks.getRoster().touchPeer(message.from_name, message.team, receiveStatus, portIndex);
    this.publishRoster();
  }

  private async handleOffer(message: FabricSessionMessage): Promise<void> {
    const identity = this.callbacks.getIdentity();
    const defaultPort = this.callbacks.getPortIndex() === 0 ? 1 : 0;
    this.callbacks.getRoster().touchPeer(message.from_name, message.team, 'open', defaultPort);
    this.publishRoster();
    if (message.to_name && message.to_name !== identity.display_name) {
      return;
    }
    if (this.busy || this.pendingInbound) {
      return;
    }

    const effective = effectiveReceiveStatus(identity.receive_status);
    if (effective === 'open') {
      this.pauseListener();
      await this.runInboundAccept(message, false);
      this.resumeListener();
      return;
    }

    this.pendingInbound = message;
    this.callbacks.patch({
      pendingOffer: toPendingOffer(message),
      waitingForPartner: true,
      bytesTotal: message.total_bytes,
      bytesDone: 0,
      statusMessage: `Incoming transfer from ${message.from_name}`,
      errorMessage: '',
    });
  }

  private handleAccept(message: FabricSessionMessage): void {
    if (!this.outboundOffer || this.outboundOffer.session_id !== message.session_id) {
      return;
    }
    this.awaitingReady = true;
    this.callbacks.patch({
      waitingForPartner: true,
      statusMessage: 'Accepted — waiting for receiver to prepare…',
    });
    this.acceptWaiter?.resolve();
    this.acceptWaiter = null;
  }

  private handleDecline(message: FabricSessionMessage): void {
    if (!this.outboundOffer || this.outboundOffer.session_id !== message.session_id) {
      return;
    }
    const from = message.from_name;
    this.outboundOffer = null;
    this.awaitingReady = false;
    this.rejectWaiters(new Error(`Declined by ${from}`));
    this.finishTransfer(false, `Transfer declined by ${from}`, 'Declined');
  }

  private handleReady(message: FabricSessionMessage): void {
    if (!this.outboundOffer || this.outboundOffer.session_id !== message.session_id) {
      return;
    }
    if (!this.awaitingReady) {
      // Accept may have been dropped on the no-buffer fabric; ready implies acceptance.
      this.awaitingReady = true;
      this.callbacks.patch({
        waitingForPartner: true,
        statusMessage: 'Accepted — waiting for receiver to prepare…',
      });
      this.acceptWaiter?.resolve();
      this.acceptWaiter = null;
    }
    this.readyWaiter?.resolve();
    this.readyWaiter = null;
    if (this.outboundOffer) {
      // Stop the listener from starting another session IN poll before sendBytes.
      // runUsb serializes IN+OUT; a stray transferIn blocks payload OUT for seconds.
      this.payloadSendPending = true;
      this.listenerPaused = true;
    }
  }

  private async maybeRetransmitOffer(): Promise<void> {
    if (this.offerRetransmitSent || !this.outboundOffer || this.awaitingReady || !this.acceptWaiter) {
      return;
    }
    this.offerRetransmitSent = true;
    try {
      console.log('[RocketBox] retransmitting offer (no accept yet)');
      this.pauseListener();
      await sleep(50);
      await this.usb.sendSessionMessage(this.outboundOffer);
      this.bumpSessionActivity();
      await sleep(this.handshake().accept_ready_gap_ms);
      this.resumeListener();
    } catch (err) {
      console.warn('[RocketBox] offer retransmit failed:', (err as Error).message);
      this.resumeListener();
    }
  }

  private waitForAccept(timeoutMs: number): Promise<void> {
    return new Promise((resolve, reject) => {
      this.acceptWaiter = { resolve, reject };
      window.setTimeout(() => {
        if (this.acceptWaiter) {
          this.acceptWaiter.reject(new Error('Accept timeout'));
          this.acceptWaiter = null;
        }
      }, timeoutMs);
    });
  }

  private waitForReady(timeoutMs: number): Promise<void> {
    return new Promise((resolve, reject) => {
      this.readyWaiter = { resolve, reject };
      window.setTimeout(() => {
        if (this.readyWaiter) {
          this.readyWaiter.reject(new Error('Ready timeout'));
          this.readyWaiter = null;
        }
      }, timeoutMs);
    });
  }

  private async sendSessionReply(
    request: FabricSessionMessage,
    kind: 'accept' | 'decline' | 'ready',
  ): Promise<void> {
    const identity = this.callbacks.getIdentity();
    const reply = buildSessionReply(request, kind, identity.display_name, identity.team);
    await this.usb.sendSessionMessage(reply);
    this.bumpSessionActivity();
  }

  private async runInboundAccept(offer: FabricSessionMessage, fromDialog: boolean): Promise<void> {
    const identity = this.callbacks.getIdentity();
    const boothDisplayRate = this.boothDisplayRate(identity);
    this.cancelDismissTimer();
    this.busy = true;
    this.pendingInbound = offer;

    this.callbacks.patch({
      busy: true,
      waitingForPartner: false,
      pendingOffer: null,
      bytesDone: 0,
      bytesTotal: offer.total_bytes,
      transferLabel: offer.payload_name,
      peakMbps: 0,
      resultMbps: 0,
      liveMbps: 0,
      boothDisplayMibS: boothDisplayRate,
      statusMessage: fromDialog ? 'Receiving…' : `Waiting for ${offer.from_name} to send…`,
      errorMessage: '',
    });

    const t0 = performance.now();
    try {
      // Let the sender's session listener re-arm after the offer before we reply.
      await sleep(this.handshake().accept_reply_delay_ms);
      await this.sendSessionReply(offer, 'accept');
      await sleep(this.handshake().accept_ready_gap_ms);
      await this.sendSessionReply(offer, 'ready');

      this.callbacks.patch({
        statusMessage: `Receiving ${offer.payload_name}…`,
        ...(boothDisplayRate > 0 ? { liveMbps: boothDisplayRate, fabricActivityMbps: boothDisplayRate } : {}),
      });

      const { data, filename } = await this.usb.receiveFileTransfer(
        this.handshake().payload_header_timeout_ms,
        (done, total) => {
          const elapsed = (performance.now() - t0) / 1000;
          this.patchTransferProgress({
            bytesDone: done,
            bytesTotal: total,
            liveMbps:
              boothDisplayRate > 0 ? boothDisplayRate : elapsed > 0 ? done / (1024 * 1024) / elapsed : 0,
            ...(boothDisplayRate > 0 ? { peakMbps: boothDisplayRate } : {}),
          });
        },
      );

      if (offer.total_bytes > 0 && data.length !== offer.total_bytes) {
        throw new Error(
          `Expected ${formatBytes(offer.total_bytes)} but received ${formatBytes(data.length)}`,
        );
      }

      const elapsed = (performance.now() - t0) / 1000;
      const resultMbps =
        boothDisplayRate > 0 ? boothDisplayRate : data.length / (1024 * 1024) / Math.max(elapsed, 0.001);
      const saveName = offer.payload_name || filename;
      this.callbacks.downloadPayload(data, saveName);
      this.finishTransfer(
        true,
        formatTransferDoneMessage(false, resultMbps),
        '',
        { peak: boothDisplayRate > 0 ? boothDisplayRate : resultMbps, result: resultMbps },
      );
    } catch (err) {
      if (!this.callbacks.isDisconnecting()) {
        this.finishTransfer(false, 'Receive failed', formatTransferError(err));
      }
    } finally {
      this.pendingInbound = null;
      this.busy = false;
      try {
        const desc = await this.usb.resetConnection();
        this.callbacks.onUsbDescription(desc);
      } catch {
        /* best effort */
      }
    }
  }

  async acceptPendingOffer(): Promise<void> {
    const offer = this.pendingInbound;
    if (!offer || !this.usb.connected) {
      return;
    }
    this.pauseListener();
    await this.runInboundAccept(offer, true);
    this.resumeListener();
  }

  async declinePendingOffer(): Promise<void> {
    const offer = this.pendingInbound;
    if (!offer) {
      return;
    }
    this.pendingInbound = null;
    this.callbacks.patch({
      pendingOffer: null,
      waitingForPartner: false,
      statusMessage: '',
      notification: 'Transfer declined',
      bytesDone: 0,
      bytesTotal: 0,
    });

    this.pauseListener();
    try {
      await this.sendSessionReply(offer, 'decline');
    } catch (err) {
      this.callbacks.patch({ errorMessage: formatTransferError(err) });
    } finally {
      try {
        const desc = await this.usb.resetConnection();
        this.callbacks.onUsbDescription(desc);
      } catch {
        /* best effort */
      }
      this.resumeListener();
    }
  }

  async resetConnection(): Promise<void> {
    this.clearTransferState();
    if (!this.usb.connected) {
      return;
    }
    const desc = await this.usb.resetConnection();
    this.callbacks.onUsbDescription(desc);
  }

  async sendToPeer(peerName: string, file: File): Promise<void> {
    if (this.busy) {
      return;
    }
    if (!this.usb.connected) {
      this.callbacks.patch({ errorMessage: 'Connect USB first' });
      return;
    }
    if (this.pendingInbound) {
      this.callbacks.patch({ errorMessage: 'Respond to the incoming file first' });
      return;
    }

    const peer = this.callbacks.getRoster().findByName(peerName);
    if (!peer) {
      this.callbacks.patch({ errorMessage: `Peer not found: ${peerName}` });
      return;
    }

    const identity = this.callbacks.getIdentity();
    const payload = await readFilePayload(file);
    const boothDisplayRate = this.boothDisplayRate(identity);

    this.pauseListener();
    await sleep(50);

    const offer: FabricSessionMessage = {
      kind: 'offer',
      session_id: makeSessionId(),
      from_name: identity.display_name,
      team: identity.team,
      to_name: peerName,
      note: '',
      payload_type: 'file',
      payload_name: file.name,
      file_count: 1,
      total_bytes: payload.length,
    };

    this.cancelDismissTimer();
    this.busy = true;
    this.outboundOffer = offer;
    this.awaitingReady = false;

    this.callbacks.patch({
      busy: true,
      waitingForPartner: true,
      errorMessage: '',
      transferLabel: file.name,
      bytesDone: 0,
      bytesTotal: file.size,
      peakMbps: 0,
      resultMbps: 0,
      liveMbps: 0,
      boothDisplayMibS: 0,
      statusMessage: `Sending offer to ${peerName}…`,
      selectedPeer: peerName,
    });

    try {
      await this.usb.sendSessionMessage(offer);
      this.bumpSessionActivity();
    } catch (err) {
      this.outboundOffer = null;
      this.busy = false;
      this.offerReceiveGraceUntil = 0;
      this.resumeListener();
      this.finishTransfer(false, 'Send failed', formatTransferError(err));
      return;
    }

    // Fabric does not buffer — keep USB IN free so the receiver can read the offer.
    this.offerReceiveGraceUntil = Date.now() + this.handshake().accept_ready_gap_ms;
    await sleep(this.handshake().accept_ready_gap_ms);
    this.resumeListener();

    const retransmitDelayMs =
      this.handshake().accept_reply_delay_ms + this.handshake().accept_ready_gap_ms + 150;
    this.offerRetransmitSent = false;
    const retransmitTimer = window.setTimeout(() => {
      void this.maybeRetransmitOffer();
    }, retransmitDelayMs);

    const acceptTimeoutSec = this.handshake().accept_timeout_sec;
    const readyTimeoutSec = this.handshake().ready_timeout_sec;
    const countdownStart = Date.now();
    const countdownInterval = window.setInterval(() => {
      const elapsed = Math.floor((Date.now() - countdownStart) / 1000);
      const remaining = Math.max(0, acceptTimeoutSec - elapsed);
      this.callbacks.patch({
        statusMessage: `Waiting for ${peerName} to accept… (${remaining}s)`,
      });
    }, 1000);
    this.callbacks.patch({
      statusMessage: `Waiting for ${peerName} to accept… (${acceptTimeoutSec}s)`,
    });

    try {
      await this.waitForAccept(acceptTimeoutSec * 1000);
      window.clearInterval(countdownInterval);
      window.clearTimeout(retransmitTimer);
      await this.waitForReady(readyTimeoutSec * 1000);
    } catch (err) {
      window.clearInterval(countdownInterval);
      window.clearTimeout(retransmitTimer);
      const msg = (err as Error).message;
      this.outboundOffer = null;
      this.busy = false;
      this.payloadSendPending = false;
      this.resumeListener();
      if (msg.includes('Declined')) {
        return;
      }
      this.finishTransfer(
        false,
        'Timed out',
        msg.includes('Accept')
          ? `No response from ${peerName} within ${acceptTimeoutSec}s`
          : `${peerName} accepted but did not start receiving in time`,
      );
      return;
    }

    this.pauseListener();
    await this.usb.prepareForPayloadSend();
    await sleep(50);

    this.callbacks.patch({
      waitingForPartner: false,
      statusMessage: `Sending ${file.name}…`,
      ...(boothDisplayRate > 0 ? { liveMbps: boothDisplayRate, fabricActivityMbps: boothDisplayRate } : {}),
    });

    const t0 = performance.now();
    try {
      await this.usb.sendBytes(
        payload,
        (done, total) => {
          const elapsed = (performance.now() - t0) / 1000;
          this.patchTransferProgress({
            bytesDone: done,
            bytesTotal: total,
            liveMbps:
              boothDisplayRate > 0 ? boothDisplayRate : elapsed > 0 ? done / (1024 * 1024) / elapsed : 0,
            ...(boothDisplayRate > 0 ? { peakMbps: boothDisplayRate } : {}),
          });
        },
        file.name,
      );
      const elapsed = (performance.now() - t0) / 1000;
      const resultMbps =
        boothDisplayRate > 0 ? boothDisplayRate : payload.length / (1024 * 1024) / Math.max(elapsed, 0.001);
      this.finishTransfer(
        true,
        formatTransferDoneMessage(true, resultMbps),
        '',
        {
          peak: boothDisplayRate > 0 ? boothDisplayRate : resultMbps,
          result: resultMbps,
        },
      );
    } catch (err) {
      this.finishTransfer(false, 'Send failed', formatTransferError(err));
    } finally {
      this.outboundOffer = null;
      this.awaitingReady = false;
      this.payloadSendPending = false;
      this.busy = false;
      try {
        const desc = await this.usb.resetConnection();
        this.callbacks.onUsbDescription(desc);
      } catch {
        /* best effort */
      }
      this.resumeListener();
    }
  }
}
