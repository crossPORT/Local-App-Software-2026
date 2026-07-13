import { rollDisplayMibS, buildAnnounceMessage, receiveStatusFromAnnounceNote } from '@rocketbox/sdk';
import { formatBytes, formatTransferDoneMessage, isTransferCompleteMessage } from './format';
import { eventLog } from './event_log';
import {
  buildSessionReply,
  type SessionMessage,
  makeSessionId,
  makeInstanceId,
} from '@rocketbox/sdk';
import { PORT_COUNT, toDisplayPort } from '@rocketbox/sdk';
import type { RocketBoxTransport, SystemInfo } from '@rocketbox/sdk';
import { PeerRoster } from './peer_roster';
import { readFilePayload } from './file_bytes';
import type { AppUiState, IdentityProfile, LinkUiState, PendingOffer, ReceiveStatus } from './types';
import { formatTransferError } from './user_errors';
import { handshakeTimingFromIdentity } from './session_handshake';
import type { HandshakeTiming } from './session_handshake';
import { TRANSFER_DONE_DISMISS_MS } from './usb_constants';
import { portIndexFromWire } from '@rocketbox/sdk';

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

export type LinkActivity = 'idle' | 'handshake' | 'receiving' | 'sending';

function effectiveReceiveStatus(status: ReceiveStatus): ReceiveStatus {
  return status === 'busy' ? 'open' : status;
}

/** USB cable leg — never trust a stale UI portIndex for echo filters. */
function portIndexOf(usb: RocketBoxTransport, fallback: number): number {
  return typeof usb.getPortIndex === 'function' ? usb.getPortIndex() : fallback;
}

function toPendingOffer(message: SessionMessage): PendingOffer {
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

export class SessionOrchestrator {
  private sessionUnsubscribe: (() => void) | null = null;
  private connectUnsubscribe: (() => void) | null = null;
  private lastAnnounceMs = 0;
  private lastAnnounceAttemptMs = 0;
  private lastAnnounceSkipReason = '';
  private announceInFlight = false;
  /** LED click while an announce is already in flight — retry once it finishes. */
  private pendingForceAnnounce = false;
  private presenceTickInFlight = false;
  private linkActivity: LinkActivity = 'idle';

  private busy = false;
  private outboundOffer: SessionMessage | null = null;
  private awaitingReady = false;
  /** Sender must not hold USB IN until the receiver has had time to read the offer. */
  private offerReceiveGraceUntil = 0;

  private pendingInbound: SessionMessage | null = null;
  /** When the current pendingInbound dialog started — used to expire a wedged
   *  offer if the receiver's dialog timer was frozen (e.g. mobile backgrounded). */
  private pendingInboundAt = 0;

  private acceptWaiter: { resolve: () => void; reject: (err: Error) => void } | null = null;
  private readyWaiter: { resolve: () => void; reject: (err: Error) => void } | null = null;
  /** Bumped when waiters are cleared so stale Accept/Ready timeouts cannot kill the next send. */
  private waiterEpoch = 0;
  private dismissTimer: number | null = null;
  private dismissEpoch = 0;
  private lastOfferRetransmitMs = 0;
  /** C++ parity: at most one offer retransmit — repeated ones keep USB IN off and drop accept. */
  private offerRetransmitted = false;
  /** Peer in the current/last transfer — refresh presence after quiet gaps. */
  private sessionPeerName = '';
  /** Stable across announces so peers don't churn roster keys every 10s. */
  private readonly instanceId = makeInstanceId();
  private presenceTimer: number | null = null;
  private retransmitTimer: number | null = null;
  private fabricActivitySeq = 0;
  private readonly completedSessionIds = new Set<string>();

  private addCompletedSession(id: string): void {
    if (!id) return;
    this.completedSessionIds.add(id);
    if (this.completedSessionIds.size > 100) {
      const oldest = this.completedSessionIds.values().next().value;
      if (oldest !== undefined) {
        this.completedSessionIds.delete(oldest);
      }
    }
  }

  constructor(
    private readonly usb: RocketBoxTransport,
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
    this.lastAnnounceAttemptMs = Date.now();
    this.lastAnnounceSkipReason = '';
    this.setLinkActivity('idle');
    this.sessionUnsubscribe?.();
    this.sessionUnsubscribe = this.usb.subscribeSession((message) => {
      this.onSessionMessage(message);
    });
    if (this.usb.subscribeConnect) {
      this.connectUnsubscribe?.();
      this.connectUnsubscribe = this.usb.subscribeConnect(() => {
        console.log('[RocketBox] Transport connection/reconnection event detected — forcing immediate announcement');
        void this.maybeSendAnnounce(true);
      });
    }
    const leg = this.callbacks.getPortIndex();
    const identity = this.callbacks.getIdentity();
    const name = identity.display_name.trim();
    eventLog(leg, 'listener_started', name || 'display_name not set');
    this.startPresenceLoop();
    this.startRetransmitLoop();
    const intervalMs = (identity.announce_interval_sec ?? 30) * 1000;
    const staggerMs = leg * (intervalMs / PORT_COUNT);
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
    this.connectUnsubscribe?.();
    this.connectUnsubscribe = null;
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
    eventLog(this.callbacks.getPortIndex(), 'link_activity', `${prev}→${next}`);
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
      await this.refreshRosterFromSdk();
      await this.maybeSendAnnounce(false);
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
    void this.releaseCircuitAfterTransfer();
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
      eventLog(this.callbacks.getPortIndex(), 'stuck_activity_reset', this.linkActivity);
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

  private applySystemsToRoster(systems: SystemInfo[]): void {
    const roster = this.callbacks.getRoster();
    for (const sys of systems) {
      const portNum = Number.parseInt(sys.id.replace(/^sys-port-/, ''), 10);
      if (!Number.isFinite(portNum) || portNum < 1) continue;
      if (sys.status === 'offline') continue;
      const peerLeg = portNum - 1;
      // Never invent "open" — that lied about peers who ask before accepting.
      const receive =
        sys.receive ?? (sys.status === 'busy' ? 'busy' : 'ask_first');
      // Do not pass sys.id as instance — that collided with real announce instance keys.
      roster.touchPeer(sys.name || sys.id, '', receive, peerLeg);
    }
    roster.markStalePeersOffline();
    this.publishRoster();
  }

  private async refreshRosterFromSdk(): Promise<void> {
    if (!this.usb.connected || !this.usb.listSystems) {
      return;
    }
    const systems = await Promise.resolve(this.usb.listSystems());
    this.applySystemsToRoster(systems);
  }

  clearTransferState(): void {
    this.cancelDismissTimer();
    this.busy = false;
    this.pendingInbound = null;
    this.pendingInboundAt = 0;
    this.outboundOffer = null;
    this.awaitingReady = false;
    this.sessionPeerName = '';
    this.offerReceiveGraceUntil = 0;
    this.offerRetransmitted = false;
    this.setLinkActivity('idle');
    this.rejectWaiters(new Error('Transfer cleared'));
  }

  private rejectWaiters(err: Error): void {
    this.waiterEpoch += 1;
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
    void this.releaseCircuitAfterTransfer();
    // Keep announcing — silencing both sides after transfer made the partner
    // fall out of the 45s roster TTL (announce_quiet).
    if (this.sessionPeerName) {
      this.notePeerAlive(this.sessionPeerName);
      this.sessionPeerName = '';
    }
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

  /** Leave EP4 aimed at last peer after transfer so reverse offers still land. */
  private async releaseCircuitAfterTransfer(): Promise<void> {
    const dest = this.usb.switchDest?.() ?? 0;
    if (dest >= 1 && dest <= 4) {
      eventLog(this.callbacks.getPortIndex(), 'transfer_leave', `dest=${dest}`);
    } else {
      eventLog(this.callbacks.getPortIndex(), 'transfer_leave', 'none');
    }
    this.clearLinkUi();
  }

  private notePeerAlive(displayName: string): void {
    const name = displayName.trim();
    if (!name) return;
    this.callbacks.getRoster().touchPeerPresence(name, '');
    this.publishRoster();
  }

  private boothDisplayRate(identity: IdentityProfile): number {
    const base = identity.booth_display_mib_s;
    return base > 0 ? rollDisplayMibS(base, identity.booth_display_jitter_pct) : 0;
  }

  private bumpSessionActivity(): void {
    this.fabricActivitySeq += 1;
    this.callbacks.patch({ fabricActivitySeq: this.fabricActivitySeq });
  }

  private setLinkUi(linkState: LinkUiState, linkedPort: number): void {
    this.callbacks.patch({
      linkState,
      linkedPort: linkState === 'none' ? 0 : linkedPort,
    });
  }

  private clearLinkUi(): void {
    this.setLinkUi('none', 0);
  }

  /** Switch toward offer sender; linking → linked when both sides can aim. */
  private async ensureCircuitTowardPeer(fromName: string): Promise<void> {
    const peer = this.callbacks.getRoster().findByName(fromName);
    if (!peer) {
      return;
    }
    const port = toDisplayPort(peer.port_index);
    this.setLinkUi('linking', port);
    try {
      await this.switchTowardPeer(peer);
      this.setLinkUi('linked', port);
      eventLog(this.callbacks.getPortIndex(), 'circuit_inbound', `dest=${port}`);
    } catch (err) {
      eventLog(
        this.callbacks.getPortIndex(),
        'circuit_inbound_fail',
        (err as Error).message,
      );
      this.clearLinkUi();
    }
  }

  /** Aim at a peer without changing the link icon (presence listen path). */
  private async aimForListen(fromName: string): Promise<void> {
    if (this.busy || this.outboundOffer || this.pendingInbound || this.linkActivity !== 'idle') {
      return;
    }
    const peer = this.callbacks.getRoster().findByName(fromName);
    if (!peer) {
      return;
    }
    try {
      await this.switchTowardPeer(peer);
      eventLog(
        this.callbacks.getPortIndex(),
        'circuit_listen',
        `dest=${toDisplayPort(peer.port_index)}`,
      );
    } catch (err) {
      eventLog(
        this.callbacks.getPortIndex(),
        'circuit_listen_fail',
        (err as Error).message,
      );
    }
  }

  private async switchTowardPeer(peer: { port_index: number; instance_id: string }): Promise<void> {
    const systemId = peer.instance_id.startsWith('sys-port-')
      ? peer.instance_id
      : `sys-port-${peer.port_index + 1}`;
    await this.usb.ensureCircuit(systemId);
  }

  private patchTransferProgress(partial: Partial<AppUiState>): void {
    const live = partial.liveMbps ?? 0;
    if (live > 0) {
      this.callbacks.patch({ ...partial, fabricActivityMbps: live });
      return;
    }
    this.callbacks.patch(partial);
  }

  private logAnnounceSkip(reason: string, force = false): void {
    // Force (LED) always logs so clicks are visible; scheduled skips stay quiet/deduped.
    if (!force && this.lastAnnounceSkipReason === reason) {
      return;
    }
    this.lastAnnounceSkipReason = reason;
    eventLog(this.callbacks.getPortIndex(), 'announce_skip', reason);
  }

  private async maybeSendAnnounce(force: boolean): Promise<void> {
    // Prefer USB fabric leg over UI ref — stale portIndex makes peers ignore us as "self".
    const leg = portIndexOf(this.usb, this.callbacks.getPortIndex());
    if (!this.sessionUnsubscribe || !this.usb.connected) {
      if (!this.usb.connected) {
        this.logAnnounceSkip('usb disconnected', force);
      }
      return;
    }
    if (this.linkActivity !== 'idle') {
      if (force) {
        this.logAnnounceSkip(`suppressed:${this.linkActivity}`, true);
      } else {
        eventLog(leg, 'announce_suppressed', this.linkActivity);
      }
      return;
    }
    if (this.outboundOffer || this.pendingInbound || this.busy) {
      this.logAnnounceSkip(
        this.busy ? 'busy' : this.outboundOffer ? 'outbound_offer' : 'pending_inbound',
        force,
      );
      return;
    }
    const identity = this.callbacks.getIdentity();
    if (!identity.display_name.trim()) {
      this.logAnnounceSkip('display_name not set', force);
      return;
    }
    if (this.announceInFlight) {
      if (force) {
        this.pendingForceAnnounce = true;
        this.logAnnounceSkip('queued', true);
      }
      return;
    }
    const now = Date.now();
    const intervalMs = (identity.announce_interval_sec ?? 30) * 1000;
    // LED/force: short gap after a *successful* announce (native parity). Scheduled: full interval.
    const forceMinGapMs = 1000;
    if (force) {
      if (this.lastAnnounceMs > 0 && now - this.lastAnnounceMs < forceMinGapMs) {
        this.logAnnounceSkip('force_throttled', true);
        return;
      }
    } else if (
      this.lastAnnounceAttemptMs > 0 &&
      now - this.lastAnnounceAttemptMs < intervalMs
    ) {
      return;
    }
    this.lastAnnounceAttemptMs = now;
    this.announceInFlight = true;
    eventLog(leg, 'announce_attempt', force ? 'forced' : 'scheduled');
    try {
      const announce = buildAnnounceMessage(
        identity.display_name,
        identity.team,
        leg,
        effectiveReceiveStatus(identity.receive_status),
        this.instanceId,
      );
      const mode = force ? 'burst' : 'rotate';
      if (this.usb.sendAnnouncePresence) {
        await this.usb.sendAnnouncePresence(announce, mode);
      } else {
        await this.usb.sendSessionMessage(announce);
      }
      await this.usb.syncSystems((systems) => {
        this.applySystemsToRoster(systems);
      });
      this.lastAnnounceMs = now;
      this.lastAnnounceSkipReason = '';
      this.bumpSessionActivity();
      this.callbacks.patch({ lastAnnounceMs: now });
      eventLog(leg, 'systems_announce', identity.display_name);
      const online = this.callbacks.getRoster().visiblePeers(true);
      const remotes = online.filter((p) => p.port_index !== leg);
      if (remotes.length === 0) {
        eventLog(leg, 'roster_waiting', 'sent ok — need peer announce inbound');
      } else {
        eventLog(leg, 'systems_synced', identity.display_name);
      }
      console.log(`[RocketBox] systems synced as "${identity.display_name}"`);
    } catch (err) {
      const message = (err as Error).message;
      eventLog(leg, 'announce_fail', message);
      console.warn('[RocketBox] announce/presence failed:', message);
    } finally {
      this.announceInFlight = false;
      if (this.pendingForceAnnounce) {
        this.pendingForceAnnounce = false;
        // Retry only if this attempt failed — success already satisfied the LED click.
        if (this.lastAnnounceMs < this.lastAnnounceAttemptMs) {
          void this.maybeSendAnnounce(true);
        }
      }
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
  private notifyIncomingOffer(message: SessionMessage): void {
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

  private onSessionMessage(message: SessionMessage): void {
    const myLeg = portIndexOf(this.usb, this.callbacks.getPortIndex());
    eventLog(
      myLeg,
      'session_received',
      `${message.kind} from=${message.from_name || '?'}`,
    );
    console.log(`[RocketBox] session message received: kind=${message.kind} from="${message.from_name}"`);
    this.bumpSessionActivity();

    if (message.from_name && message.kind !== 'announce') {
      this.notePeerAlive(message.from_name);
    }

    if (message.session_id && this.completedSessionIds.has(message.session_id)) {
      console.log(`[RocketBox] session message ignored: session_id "${message.session_id}" was already completed/handled`);
      return;
    }

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
      case 'announce': {
        const portMatch = message.note?.match(/(?:^|;)\s*port=(\d+)/);
        const wire = portMatch ? Number.parseInt(portMatch[1]!, 10) : NaN;
        const peerLeg = portIndexFromWire(wire);
        if (peerLeg == null || peerLeg === myLeg) {
          eventLog(
            myLeg,
            'announce_echo_reject',
            `${message.from_name || '?'} port=${Number.isFinite(wire) ? wire : '?'}`,
          );
          break;
        }
        const receive = receiveStatusFromAnnounceNote(message.note);
        const instance = message.note?.match(/instance=([^;]+)/)?.[1] ?? '';
        this.callbacks
          .getRoster()
          .touchPeer(message.from_name, message.team, receive, peerLeg, instance);
        this.publishRoster();
        eventLog(
          myLeg,
          'announce_received',
          `${message.from_name} port=${peerLeg + 1}`,
        );
        void this.aimForListen(message.from_name);
        break;
      }
      default:
        break;
    }
  }

  private async handleOffer(message: SessionMessage): Promise<void> {
    const identity = this.callbacks.getIdentity();
    const myLeg = this.callbacks.getPortIndex();
    this.sessionPeerName = message.from_name;

    // Check if the offer specifically targets our physical port index (to_port=X)
    const toPortMatch = message.note?.match(/to_port=(\d+)/);
    if (toPortMatch) {
      const targetPortIndex = parseInt(toPortMatch[1], 10);
      if (targetPortIndex !== myLeg) {
        console.log(`[RocketBox] offer ignored: addressed to Port ${targetPortIndex + 1}, our physical port is Port ${myLeg + 1}`);
        return;
      }
    } else if (message.to_name && message.to_name !== identity.display_name) {
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
        await this.ensureCircuitTowardPeer(message.from_name);
        await this.runInboundAccept(message, false);
      } finally {
        this.resumeListener();
      }
      return;
    }

    console.log(`[RocketBox] incoming offer from "${message.from_name}": ${message.payload_name}`);
    await this.ensureCircuitTowardPeer(message.from_name);
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

  private handleAccept(message: SessionMessage): void {
    if (!this.outboundOffer || this.outboundOffer.session_id !== message.session_id) {
      return;
    }
    this.awaitingReady = true;
    const peer = this.callbacks.getRoster().findByName(message.from_name);
    if (peer) {
      this.setLinkUi('linked', toDisplayPort(peer.port_index));
    }
    this.callbacks.patch({
      waitingForPartner: true,
      statusMessage: 'Accepted — waiting for receiver to prepare…',
    });
    this.acceptWaiter?.resolve();
    this.acceptWaiter = null;
  }

  private handleDecline(message: SessionMessage): void {
    if (!this.outboundOffer || this.outboundOffer.session_id !== message.session_id) {
      return;
    }
    const from = message.from_name;
    this.outboundOffer = null;
    this.awaitingReady = false;
    this.rejectWaiters(new Error(`Declined by ${from}`));
    this.finishTransfer(false, `Transfer declined by ${from}`, 'Declined');
  }

  private handleReady(message: SessionMessage): void {
    if (!this.outboundOffer || this.outboundOffer.session_id !== message.session_id) {
      return;
    }
    if (!this.awaitingReady) {
      // Accept may have been dropped on the no-buffer fabric; ready implies acceptance.
      this.awaitingReady = true;
      const peer = this.callbacks.getRoster().findByName(message.from_name);
      if (peer) {
        this.setLinkUi('linked', toDisplayPort(peer.port_index));
      }
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
    if (this.offerRetransmitted || Date.now() < this.offerReceiveGraceUntil) {
      return;
    }
    const intervalMs =
      this.handshake().accept_reply_delay_ms + this.handshake().accept_ready_gap_ms + 150;
    const now = Date.now();
    if (now - this.lastOfferRetransmitMs < intervalMs) {
      return;
    }
    this.offerRetransmitted = true;
    this.lastOfferRetransmitMs = now;
    try {
      this.pauseListener();
      await sleep(50);
      // Accept may arrive while pausing IN — do not clobber the ready path.
      if (this.awaitingReady || !this.outboundOffer || !this.acceptWaiter) {
        this.resumeListener();
        return;
      }
      console.log('[RocketBox] retransmitting offer once (no accept yet)');
      eventLog(this.callbacks.getPortIndex(), 'offer_retransmit', this.outboundOffer.session_id);
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
    const epoch = this.waiterEpoch;
    return new Promise((resolve, reject) => {
      this.acceptWaiter = { resolve, reject };
      setTimeout(() => {
        if (this.waiterEpoch !== epoch || !this.acceptWaiter) {
          return;
        }
        this.acceptWaiter.reject(new Error('Accept timeout'));
        this.acceptWaiter = null;
      }, timeoutMs);
    });
  }

  private waitForReady(timeoutMs: number): Promise<void> {
    const epoch = this.waiterEpoch;
    return new Promise((resolve, reject) => {
      this.readyWaiter = { resolve, reject };
      setTimeout(() => {
        if (this.waiterEpoch !== epoch || !this.readyWaiter) {
          return;
        }
        this.readyWaiter.reject(new Error('Ready timeout'));
        this.readyWaiter = null;
      }, timeoutMs);
    });
  }

  private async sendSessionReply(
    request: SessionMessage,
    kind: 'accept' | 'decline' | 'ready',
  ): Promise<void> {
    const identity = this.callbacks.getIdentity();
    const reply = buildSessionReply(request, kind, identity.display_name, identity.team);
    await this.usb.sendSessionMessage(reply);
    this.bumpSessionActivity();
  }

  private async sendAcceptReady(offer: SessionMessage): Promise<void> {
    await this.sendSessionReply(offer, 'accept');
    await sleep(this.handshake().accept_ready_gap_ms);
    await this.sendSessionReply(offer, 'ready');
  }

  private async runInboundAccept(offer: SessionMessage, fromDialog: boolean): Promise<void> {
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
      // Let the sender re-arm USB IN after the offer (and one possible retransmit).
      await sleep(this.handshake().accept_reply_delay_ms);
      await this.sendAcceptReady(offer);
      // Re-aim after accept/ready USB churn so the payload path is live.
      await this.ensureCircuitTowardPeer(offer.from_name);
      await sleep(30);

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
      // Always finish — skipping left sticky switch + "Receiving…" forever.
      this.finishTransfer(false, 'Receive failed', formatTransferError(err));
    } finally {
      if (offer?.session_id) {
        this.addCompletedSession(offer.session_id);
      }
      this.pendingInbound = null;
      this.pendingInboundAt = 0;
      try {
        await this.usb.waitForIdle();
      } catch {
        /* best effort */
      }
      this.busy = false;
      this.setLinkActivity('idle');
      this.resumeListener();
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
      await this.releaseCircuitAfterTransfer();
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

  /** User released EP4 link via roster icon (after confirm). */
  async releaseLinkedCircuit(): Promise<void> {
    const leg = this.callbacks.getPortIndex();
    eventLog(leg, 'link_release', 'user');
    this.outboundOffer = null;
    this.awaitingReady = false;
    this.rejectWaiters(new Error('Link released'));
    this.busy = false;
    this.setLinkActivity('idle');
    this.resumeListener();
    if (this.usb.clearCircuit) {
      try {
        await this.usb.clearCircuit();
        eventLog(leg, 'link_clear', 'dest=0');
      } catch (err) {
        eventLog(leg, 'link_clear_fail', (err as Error).message);
      }
    }
    this.clearLinkUi();
    this.callbacks.patch({
      busy: false,
      waitingForPartner: false,
      statusMessage: 'Link released',
      errorMessage: '',
    });
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

    const offer: SessionMessage = {
      kind: 'offer',
      session_id: makeSessionId(),
      from_name: identity.display_name,
      team: identity.team,
      to_name: peerName,
      note: `to_port=${peer.port_index}`,
      payload_type: 'file',
      payload_name: file.name,
      file_count: 1,
      total_bytes: payload.length,
    };

    this.cancelDismissTimer();
    this.busy = true;
    this.outboundOffer = offer;
    this.awaitingReady = false;
    this.sessionPeerName = peerName;

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
      const systemId =
        peer.instance_id.startsWith('sys-port-')
          ? peer.instance_id
          : `sys-port-${peer.port_index + 1}`;
      await this.usb.ensureCircuit(systemId);
      this.setLinkUi('linking', toDisplayPort(peer.port_index));
      await this.usb.sendSessionMessage(offer);
      this.bumpSessionActivity();
    } catch (err) {
      this.outboundOffer = null;
      this.busy = false;
      this.offerReceiveGraceUntil = 0;
      this.resumeListener();
      this.clearLinkUi();
      this.finishTransfer(false, 'Send failed', formatTransferError(err));
      return;
    }

    // Fabric does not buffer — keep USB IN free so the receiver can read the offer.
    this.offerReceiveGraceUntil = Date.now() + this.handshake().accept_ready_gap_ms;
    this.lastOfferRetransmitMs = Date.now();
    this.offerRetransmitted = false;
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
    // Receiver arms IN right after ready; short gap is enough (long payload header timeout).
    await sleep(this.handshake().accept_ready_gap_ms);

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
      if (offer?.session_id) {
        this.addCompletedSession(offer.session_id);
      }
      this.outboundOffer = null;
      this.awaitingReady = false;
      try {
        await this.usb.waitForIdle();
      } catch {
        /* best effort */
      }
      this.busy = false;
      this.setLinkActivity('idle');
      this.resumeListener();
    }
  }
}
