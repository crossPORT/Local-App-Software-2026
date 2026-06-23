import { rollBoothDisplayMibS } from './fabric_protocol';
import { formatBytes, formatTransferDoneMessage, isTransferCompleteMessage } from './format';
import { parseAnnounceNote, resolveRemoteFabricLeg } from './announce_note';
import { boothLog } from './booth_log';
import { displayPortFromLeg } from './fabric_port';
import {
  buildAnnounceMessage,
  buildSessionReply,
  makeInstanceId,
  type FabricSessionMessage,
  makeSessionId,
} from './fabric_session';
import { FABRIC_LEG_COUNT } from './fabric_port';
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

export type LinkActivity = 'idle' | 'handshake' | 'receiving' | 'sending';

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
  private sessionUnsubscribe: (() => void) | null = null;
  private lastAnnounceMs = 0;
  private lastAnnounceAttemptMs = 0;
  private lastAnnounceSkipReason = '';
  private announceInFlight = false;
  private presenceTickInFlight = false;
  private linkActivity: LinkActivity = 'idle';

  private busy = false;
  private outboundOffer: FabricSessionMessage | null = null;
  private awaitingReady = false;
  /** Sender must not hold USB IN until the receiver has had time to read the offer. */
  private offerReceiveGraceUntil = 0;

  private pendingInbound: FabricSessionMessage | null = null;
  /** When the current pendingInbound dialog started — used to expire a wedged
   *  offer if the receiver's dialog timer was frozen (e.g. mobile backgrounded). */
  private pendingInboundAt = 0;

  private acceptWaiter: { resolve: () => void; reject: (err: Error) => void } | null = null;
  private readyWaiter: { resolve: () => void; reject: (err: Error) => void } | null = null;
  private dismissTimer: number | null = null;
  private dismissEpoch = 0;
  private lastOfferRetransmitMs = 0;
  private presenceTimer: number | null = null;
  private retransmitTimer: number | null = null;
  private fabricActivitySeq = 0;
  private readonly instanceId = makeInstanceId();

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
    this.lastAnnounceMs = 0;
    this.lastAnnounceSkipReason = '';
    this.setLinkActivity('idle');
    this.sessionUnsubscribe?.();
    this.sessionUnsubscribe = this.usb.subscribeSession((message) => {
      this.onSessionMessage(message);
    });
    const leg = this.callbacks.getPortIndex();
    const name = this.callbacks.getIdentity().display_name.trim();
    boothLog(leg, 'listener_started', name || 'display_name not set');
    this.startPresenceLoop();
    this.startRetransmitLoop();
    const staggerMs = leg * (ANNOUNCE_INTERVAL_MS / FABRIC_LEG_COUNT);
    void (async () => {
      await sleep(staggerMs + Math.floor(Math.random() * ANNOUNCE_JITTER_MS));
      if (this.sessionUnsubscribe) {
        await this.maybeSendAnnounce(true);
      }
    })();
  }

  stopListener(): void {
    this.sessionUnsubscribe?.();
    this.sessionUnsubscribe = null;
    this.stopPresenceLoop();
    this.stopRetransmitLoop();
    this.usb.setListenMode('off');
  }

  private startRetransmitLoop(): void {
    this.stopRetransmitLoop();
    this.retransmitTimer = window.setInterval(() => {
      void this.maybeRetransmitOffer();
    }, 100);
  }

  private stopRetransmitLoop(): void {
    if (this.retransmitTimer !== null) {
      window.clearInterval(this.retransmitTimer);
      this.retransmitTimer = null;
    }
  }

  private setLinkActivity(next: LinkActivity): void {
    if (this.linkActivity === next) {
      this.syncListenMode();
      return;
    }
    const prev = this.linkActivity;
    this.linkActivity = next;
    boothLog(this.callbacks.getPortIndex(), 'link_activity', `${prev}→${next}`);
    this.syncListenMode();
  }

  private syncListenMode(): void {
    if (this.linkActivity === 'idle') {
      this.usb.setListenMode('always');
    } else if (this.linkActivity === 'handshake') {
      this.usb.setListenMode('handshake');
    } else {
      this.usb.setListenMode('off');
    }
  }

  private startPresenceLoop(): void {
    this.stopPresenceLoop();
    void this.tickPresence(true);
    this.presenceTimer = window.setInterval(() => {
      void this.tickPresence(false);
    }, 1000);
  }

  private stopPresenceLoop(): void {
    if (this.presenceTimer !== null) {
      window.clearInterval(this.presenceTimer);
      this.presenceTimer = null;
    }
  }

  private async tickPresence(_initial: boolean): Promise<void> {
    if (this.presenceTickInFlight) {
      return;
    }
    this.presenceTickInFlight = true;
    try {
      if (!this.sessionUnsubscribe || !this.usb.connected) {
        return;
      }
      this.expireStalePendingOffer();
      this.recoverStuckHandshakeState();
      this.usb.ensureListening();
      const overdue =
        this.lastAnnounceMs > 0 &&
        Date.now() - this.lastAnnounceMs >= ANNOUNCE_INTERVAL_MS;
      await this.maybeSendAnnounce(overdue);
    } finally {
      this.presenceTickInFlight = false;
    }
  }

  /**
   * Clear an incoming-offer dialog that was never answered. The receiver's
   * dialog has its own auto-decline timer, but on mobile that timer is frozen
   * while Chrome is backgrounded — leaving `pendingInbound` wedged, which both
   * blocks announces and makes us silently reject every new offer. This is the
   * orchestrator-owned safety net that runs regardless of the UI timer.
   */
  private expireStalePendingOffer(): void {
    if (!this.pendingInbound || this.busy || this.pendingInboundAt === 0) {
      return;
    }
    const maxAgeMs = this.handshake().accept_dialog_sec * 1000 + 5000;
    if (Date.now() - this.pendingInboundAt <= maxAgeMs) {
      return;
    }
    console.warn('[RocketBox] expiring unanswered incoming offer');
    this.pendingInbound = null;
    this.pendingInboundAt = 0;
    this.callbacks.patch({
      pendingOffer: null,
      statusMessage: '',
      notification: '',
      bytesDone: 0,
      bytesTotal: 0,
    });
  }

  /**
   * `linkActivity` is only valid while a transfer is in flight. If we are idle
   * (no offer, not busy, no inbound) but activity is still non-idle, an aborted
   * handshake left it stuck — which silently suppresses announces forever.
   */
  private recoverStuckHandshakeState(): void {
    const idle = !this.busy && !this.outboundOffer && !this.pendingInbound;
    if (idle && this.linkActivity !== 'idle') {
      console.warn('[RocketBox] clearing stuck link activity while idle');
      boothLog(this.callbacks.getPortIndex(), 'stuck_activity_reset', this.linkActivity);
      this.setLinkActivity('idle');
    }
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
    this.pendingInboundAt = 0;
    this.outboundOffer = null;
    this.awaitingReady = false;
    this.offerReceiveGraceUntil = 0;
    this.setLinkActivity('idle');
    this.rejectWaiters(new Error('Transfer cleared'));
  }

  private rejectWaiters(err: Error): void {
    this.acceptWaiter?.reject(err);
    this.readyWaiter?.reject(err);
    this.acceptWaiter = null;
    this.readyWaiter = null;
  }

  private pauseListener(): void {
    this.usb.setListenMode('off');
  }

  private resumeListener(): void {
    this.offerReceiveGraceUntil = 0;
    this.syncListenMode();
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
    this.setLinkActivity('idle');
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

  private logAnnounceSkip(reason: string): void {
    if (this.lastAnnounceSkipReason === reason) {
      return;
    }
    this.lastAnnounceSkipReason = reason;
    boothLog(this.callbacks.getPortIndex(), 'announce_skip', reason);
  }

  private async maybeSendAnnounce(force: boolean): Promise<void> {
    const leg = this.callbacks.getPortIndex();
    if (!this.sessionUnsubscribe || !this.usb.connected) {
      if (!this.usb.connected) {
        this.logAnnounceSkip('usb disconnected');
      }
      return;
    }
    if (this.linkActivity !== 'idle') {
      boothLog(leg, 'announce_suppressed', this.linkActivity);
      return;
    }
    if (this.outboundOffer || this.pendingInbound || this.busy) {
      this.logAnnounceSkip(
        this.busy ? 'busy' : this.outboundOffer ? 'outbound_offer' : 'pending_inbound',
      );
      return;
    }
    const identity = this.callbacks.getIdentity();
    if (!identity.display_name.trim()) {
      this.logAnnounceSkip('display_name not set');
      return;
    }
    if (this.announceInFlight) {
      return;
    }
    const now = Date.now();
    if (!force && now - this.lastAnnounceAttemptMs < ANNOUNCE_INTERVAL_MS) {
      return;
    }
    this.lastAnnounceAttemptMs = now;
    this.announceInFlight = true;
    boothLog(leg, 'announce_attempt', force ? 'forced' : 'scheduled');
    try {
      const message = buildAnnounceMessage(identity, leg, this.instanceId);
      await this.usb.sendSessionMessage(message);
      this.lastAnnounceMs = now;
      this.lastAnnounceSkipReason = '';
      this.bumpSessionActivity();
      this.callbacks.patch({ lastAnnounceMs: now });
      boothLog(leg, 'announce_sent', identity.display_name);
      console.log(`[RocketBox] announce SENT as "${identity.display_name}"`);
    } catch (err) {
      const message = (err as Error).message;
      boothLog(leg, 'announce_fail', message);
      console.warn('[RocketBox] announce send failed:', message);
    } finally {
      this.announceInFlight = false;
    }
  }

  private handshake(): HandshakeTiming {
    return handshakeTimingFromIdentity(this.callbacks.getIdentity());
  }

  /**
   * Best-effort attention grab. Must never throw: on Android Chrome the
   * `Notification` constructor throws ("Illegal constructor" — it requires
   * ServiceWorkerRegistration.showNotification()), and a throw here would abort
   * offer handling before the receive dialog is shown.
   */
  private notifyIncomingOffer(message: FabricSessionMessage): void {
    const body = `${message.from_name} wants to send ${message.payload_name || 'a file'}`;
    try {
      if (typeof Notification !== 'undefined' && Notification.permission === 'granted') {
        new Notification('Incoming file', { body, tag: 'rocketbox-offer' });
      }
    } catch (err) {
      console.debug('[RocketBox] notification unavailable:', (err as Error).message);
    }
    try {
      if (typeof navigator !== 'undefined' && typeof navigator.vibrate === 'function') {
        navigator.vibrate([200, 100, 200]);
      }
    } catch {
      /* vibration not supported — ignore */
    }
  }

  private onSessionMessage(message: FabricSessionMessage): void {
    boothLog(
      this.callbacks.getPortIndex(),
      'session_received',
      `${message.kind} from=${message.from_name || '?'}`,
    );
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
    if (!message.from_name) {
      return;
    }

    const myLeg = this.callbacks.getPortIndex();
    const defaultLeg = (myLeg + 1) % FABRIC_LEG_COUNT;
    const parsed = parseAnnounceNote(message.note, defaultLeg);

    if (parsed.instanceId && parsed.instanceId === this.instanceId) {
      console.debug('[RocketBox] announce ignored (own instance)');
      return;
    }
    if (
      !parsed.instanceId &&
      message.from_name === identity.display_name &&
      parsed.portIndex === myLeg
    ) {
      console.debug('[RocketBox] announce ignored (own station, legacy)');
      return;
    }

    const peerLeg = resolveRemoteFabricLeg(myLeg, parsed.portIndex);
    if (peerLeg === null) {
      boothLog(myLeg, 'announce_echo_reject', `port=${displayPortFromLeg(parsed.portIndex)}`);
      return;
    }
    console.log(`[RocketBox] announce RECEIVED from "${message.from_name}" (leg ${peerLeg})`);
    boothLog(myLeg, 'announce_received', `${message.from_name} port=${displayPortFromLeg(peerLeg)}`);
    this.callbacks
      .getRoster()
      .touchPeer(message.from_name, message.team, parsed.receiveStatus, peerLeg, parsed.instanceId);
    this.publishRoster();
  }

  private async handleOffer(message: FabricSessionMessage): Promise<void> {
    const identity = this.callbacks.getIdentity();
    if (message.to_name && message.to_name !== identity.display_name) {
      console.warn(
        `[RocketBox] offer ignored: addressed to "${message.to_name}", local name is "${identity.display_name}"`,
      );
      return;
    }
    if (this.busy) {
      console.warn('[RocketBox] offer ignored: already receiving/sending a file');
      return;
    }
    if (this.pendingInbound) {
      // Same offer retransmitted on the no-buffer link — keep the live dialog,
      // just refresh its expiry. Don't re-notify/vibrate on every retransmit.
      if (this.pendingInbound.session_id === message.session_id) {
        this.pendingInboundAt = Date.now();
        return;
      }
      // A different offer arrived while an old one is still pending and we are
      // not actively transferring — the previous dialog was abandoned/stale
      // (e.g. mobile froze its auto-decline timer). The newest offer wins.
      console.warn('[RocketBox] replacing stale pending offer with newer one');
    }

    const effective = effectiveReceiveStatus(identity.receive_status);
    if (effective === 'open') {
      this.pauseListener();
      try {
        await this.runInboundAccept(message, false);
      } finally {
        this.resumeListener();
      }
      return;
    }

    console.log(`[RocketBox] incoming offer from "${message.from_name}": ${message.payload_name}`);
    this.pendingInbound = message;
    this.pendingInboundAt = Date.now();
    // Show the dialog BEFORE attempting any notification — on Android Chrome the
    // Notification constructor throws ("Illegal constructor"), and if that ran
    // first it would abort before the dialog was ever rendered.
    this.callbacks.patch({
      pendingOffer: toPendingOffer(message),
      waitingForPartner: false,
      bytesTotal: message.total_bytes,
      bytesDone: 0,
      statusMessage: `Incoming transfer from ${message.from_name}`,
      errorMessage: '',
      notification: `Incoming file from ${message.from_name}`,
    });
    this.notifyIncomingOffer(message);
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
      this.setLinkActivity('sending');
    }
  }

  private async maybeRetransmitOffer(): Promise<void> {
    if (!this.outboundOffer || this.awaitingReady || !this.acceptWaiter) {
      return;
    }
    if (Date.now() < this.offerReceiveGraceUntil) {
      return;
    }
    const intervalMs =
      this.handshake().accept_reply_delay_ms + this.handshake().accept_ready_gap_ms + 150;
    const now = Date.now();
    if (now - this.lastOfferRetransmitMs < intervalMs) {
      return;
    }
    this.lastOfferRetransmitMs = now;
    try {
      console.log('[RocketBox] retransmitting offer (no accept yet)');
      boothLog(this.callbacks.getPortIndex(), 'offer_retransmit', this.outboundOffer.session_id);
      this.pauseListener();
      await sleep(50);
      await this.usb.sendSessionMessage(this.outboundOffer);
      this.bumpSessionActivity();
      this.offerReceiveGraceUntil = Date.now() + this.handshake().accept_ready_gap_ms;
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
    this.setLinkActivity('receiving');

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
      });

      const { data, filename } = await this.usb.receiveFileTransfer(
        this.handshake().payload_header_timeout_ms,
        offer.total_bytes,
        (done, total) => {
          const elapsed = (performance.now() - t0) / 1000;
          const measured = elapsed > 0 ? done / (1024 * 1024) / elapsed : 0;
          this.patchTransferProgress({
            bytesDone: done,
            bytesTotal: total,
            liveMbps:
              boothDisplayRate > 0 && done > 0 ? boothDisplayRate : measured,
            ...(boothDisplayRate > 0 && done > 0 ? { peakMbps: boothDisplayRate } : {}),
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
      this.pendingInboundAt = 0;
      this.busy = false;
      this.setLinkActivity('idle');
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
    try {
      await this.runInboundAccept(offer, true);
    } finally {
      this.resumeListener();
    }
  }

  async declinePendingOffer(): Promise<void> {
    const offer = this.pendingInbound;
    if (!offer) {
      return;
    }
    this.pendingInbound = null;
    this.pendingInboundAt = 0;
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

  async sendToPeer(peerId: string, file: File): Promise<void> {
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

    const peer = this.callbacks.getRoster().findById(peerId);
    if (!peer) {
      this.callbacks.patch({ errorMessage: 'Peer not found — refresh the roster and try again' });
      return;
    }

    const peerName = peer.display_name;

    const identity = this.callbacks.getIdentity();
    const payload = await readFilePayload(file);
    const boothDisplayRate = this.boothDisplayRate(identity);

    this.pauseListener();
    await sleep(50);
    this.setLinkActivity('handshake');

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
      selectedPeer: peerId,
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
    this.lastOfferRetransmitMs = Date.now();
    await sleep(this.handshake().accept_ready_gap_ms);
    this.setLinkActivity('handshake');
    this.resumeListener();

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
      await this.waitForReady(readyTimeoutSec * 1000);
    } catch (err) {
      window.clearInterval(countdownInterval);
      const msg = (err as Error).message;
      this.outboundOffer = null;
      this.busy = false;
      this.setLinkActivity('idle');
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

    this.setLinkActivity('sending');
    await this.usb.prepareForPayloadSend();
    await sleep(50);

    this.callbacks.patch({
      waitingForPartner: false,
      statusMessage: `Sending ${file.name}…`,
    });

    const t0 = performance.now();
    try {
      await this.usb.sendBytes(
        payload,
        (done, total) => {
          const elapsed = (performance.now() - t0) / 1000;
          const measured = elapsed > 0 ? done / (1024 * 1024) / elapsed : 0;
          this.patchTransferProgress({
            bytesDone: done,
            bytesTotal: total,
            liveMbps:
              boothDisplayRate > 0 && done > 0 ? boothDisplayRate : measured,
            ...(boothDisplayRate > 0 && done > 0 ? { peakMbps: boothDisplayRate } : {}),
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
      this.busy = false;
      this.setLinkActivity('idle');
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
