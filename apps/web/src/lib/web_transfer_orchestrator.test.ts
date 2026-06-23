import { describe, expect, it, vi } from 'vitest';
import { ANNOUNCE_INTERVAL_MS, WebTransferOrchestrator } from './web_transfer_orchestrator';
import type { OrchestratorCallbacks } from './web_transfer_orchestrator';
import type { FabricTransport } from './fabric_transport';
import type { FabricSessionMessage } from './fabric_session';
import { PeerRoster } from './peer_roster';
import { defaultIdentityProfile } from './config';
import type { AppUiState } from './types';

function makeTransport(): FabricTransport & {
  sent: FabricSessionMessage[];
  sessionHandler: ((message: FabricSessionMessage) => void) | null;
} {
  const sent: FabricSessionMessage[] = [];
  let sessionHandler: ((message: FabricSessionMessage) => void) | null = null;
  return {
    sent,
    get sessionHandler() {
      return sessionHandler;
    },
    set sessionHandler(v) {
      sessionHandler = v;
    },
    connected: true,
    getFabricPortIndex: () => 0,
    getFabricLeg: () => 0,
    getSerialNumber: () => '0000000000000001',
    connect: async () => '',
    reconnectKnown: async () => '',
    describeDevice: () => '',
    disconnect: async () => {},
    forgetThisDevice: async () => {},
    resetConnection: async () => '',
    ownsDevice: () => false,
    markDisconnected: () => {},
    setListenMode: () => {},
    ensureListening: () => {},
    subscribeSession: (handler) => {
      sessionHandler = handler;
      return () => {
        sessionHandler = null;
      };
    },
    waitForIdle: async () => {},
    sendBytes: async () => {},
    receiveHeader: async () => ({ fileSize: 0, filename: '', frameKind: 'payload' as const }),
    receivePayload: async () => new Uint8Array(),
    discardPayload: async () => {},
    receiveBytes: async () => new Uint8Array(),
    sendSessionMessage: async (message: FabricSessionMessage) => {
      sent.push(message);
    },
    tryReceiveSessionMessage: async () => null,
    receiveFileTransfer: async () => ({ data: new Uint8Array(), filename: '' }),
    prepareForPayloadSend: async () => {},
  };
}

function makeOffer(sessionId: string, from = 'Alice'): FabricSessionMessage {
  return {
    kind: 'offer',
    session_id: sessionId,
    from_name: from,
    team: '',
    to_name: 'Sally',
    note: '',
    payload_type: 'file',
    payload_name: 'photo.jpg',
    file_count: 1,
    total_bytes: 4096,
  };
}

function makeOrchestrator(receiveStatus: 'open' | 'ask_first' = 'ask_first') {
  const transport = makeTransport();
  const roster = new PeerRoster();
  const identity = { ...defaultIdentityProfile(0), display_name: 'Sally', receive_status: receiveStatus };
  const patch = vi.fn<[Partial<AppUiState>], void>();
  const callbacks: OrchestratorCallbacks = {
    patch,
    getIdentity: () => identity,
    getPortIndex: () => 0,
    getRoster: () => roster,
    onUsbDescription: () => {},
    downloadPayload: () => {},
    isDisconnecting: () => false,
  };
  const orchestrator = new WebTransferOrchestrator(transport, callbacks);
  return { orchestrator, transport, patch };
}

describe('WebTransferOrchestrator presence recovery', () => {
  it('does not stack announces while lastAnnounceMs is still zero', async () => {
    const { orchestrator, transport } = makeOrchestrator();
    const orch = orchestrator as unknown as {
      lastAnnounceMs: number;
      sessionUnsubscribe: (() => void) | null;
      tickPresence: (initial: boolean) => Promise<void>;
    };
    orch.sessionUnsubscribe = () => {};
    orch.lastAnnounceMs = 0;

    let releaseSend: (() => void) | null = null;
    const sendGate = new Promise<void>((resolve) => {
      releaseSend = resolve;
    });
    transport.sendSessionMessage = async (message: FabricSessionMessage) => {
      transport.sent.push(message);
      await sendGate;
    };

    const first = orch.tickPresence(false);
    const second = orch.tickPresence(false);
    await Promise.resolve();
    expect(transport.sent.filter((m) => m.kind === 'announce')).toHaveLength(1);

    releaseSend?.();
    await first;
    await second;
    expect(transport.sent.filter((m) => m.kind === 'announce')).toHaveLength(1);
  });

  it('resumes announcing after a stuck link activity while idle', async () => {
    const { orchestrator, transport, patch } = makeOrchestrator();
    const orch = orchestrator as unknown as {
      linkActivity: string;
      lastAnnounceMs: number;
      sessionUnsubscribe: (() => void) | null;
      tickPresence: (initial: boolean) => Promise<void>;
    };

    orch.linkActivity = 'sending';
    orch.lastAnnounceMs = Date.now() - ANNOUNCE_INTERVAL_MS * 3;
    orch.sessionUnsubscribe = () => {};

    await orch.tickPresence(false);

    expect(orch.linkActivity).toBe('idle');
    expect(transport.sent.some((m) => m.kind === 'announce')).toBe(true);
    expect(patch).toHaveBeenCalledWith(expect.objectContaining({ lastAnnounceMs: expect.any(Number) }));
  });

  it('does not clear activity during an active outbound transfer', async () => {
    const { orchestrator, transport } = makeOrchestrator();
    const orch = orchestrator as unknown as {
      linkActivity: string;
      busy: boolean;
      tickPresence: (initial: boolean) => Promise<void>;
    };

    orch.busy = true;
    orch.linkActivity = 'sending';

    await orch.tickPresence(false);

    expect(orch.linkActivity).toBe('sending');
    expect(transport.sent.some((m) => m.kind === 'announce')).toBe(false);
  });
});

describe('WebTransferOrchestrator incoming offer handling', () => {
  it('replaces a stale pending offer with a newer one instead of silently rejecting', async () => {
    const { orchestrator, patch } = makeOrchestrator('ask_first');
    const orch = orchestrator as unknown as {
      pendingInbound: FabricSessionMessage | null;
      pendingInboundAt: number;
      handleOffer: (message: FabricSessionMessage) => Promise<void>;
    };

    orch.pendingInbound = makeOffer('old-session');
    orch.pendingInboundAt = Date.now() - 1000;

    await orch.handleOffer(makeOffer('new-session'));

    expect(orch.pendingInbound?.session_id).toBe('new-session');
    expect(patch).toHaveBeenCalledWith(
      expect.objectContaining({ pendingOffer: expect.objectContaining({ session_id: 'new-session' }) }),
    );
  });

  it('refreshes (does not re-notify) when the same offer is retransmitted', async () => {
    const { orchestrator, patch } = makeOrchestrator('ask_first');
    const orch = orchestrator as unknown as {
      pendingInbound: FabricSessionMessage | null;
      pendingInboundAt: number;
      handleOffer: (message: FabricSessionMessage) => Promise<void>;
    };

    orch.pendingInbound = makeOffer('same-session');
    orch.pendingInboundAt = 1;
    patch.mockClear();

    await orch.handleOffer(makeOffer('same-session'));

    expect(orch.pendingInboundAt).toBeGreaterThan(1);
    expect(patch).not.toHaveBeenCalled();
  });

  it('still shows the receive dialog when Notification throws (Android Chrome)', async () => {
    const original = (globalThis as { Notification?: unknown }).Notification;
    class ThrowingNotification {
      static permission = 'granted';
      constructor() {
        throw new TypeError("Failed to construct 'Notification': Illegal constructor.");
      }
    }
    (globalThis as { Notification?: unknown }).Notification = ThrowingNotification;
    try {
      const { orchestrator, patch } = makeOrchestrator('ask_first');
      const orch = orchestrator as unknown as {
        pendingInbound: FabricSessionMessage | null;
        handleOffer: (message: FabricSessionMessage) => Promise<void>;
      };

      await orch.handleOffer(makeOffer('notif-session'));

      expect(orch.pendingInbound?.session_id).toBe('notif-session');
      expect(patch).toHaveBeenCalledWith(
        expect.objectContaining({ pendingOffer: expect.objectContaining({ session_id: 'notif-session' }) }),
      );
    } finally {
      (globalThis as { Notification?: unknown }).Notification = original;
    }
  });

  it('expires a wedged unanswered offer so announcing resumes', async () => {
    const { orchestrator, transport, patch } = makeOrchestrator('ask_first');
    const orch = orchestrator as unknown as {
      pendingInbound: FabricSessionMessage | null;
      pendingInboundAt: number;
      lastAnnounceMs: number;
      sessionUnsubscribe: (() => void) | null;
      tickPresence: (initial: boolean) => Promise<void>;
    };

    orch.pendingInbound = makeOffer('wedged-session');
    orch.pendingInboundAt = Date.now() - 70_000;
    orch.lastAnnounceMs = Date.now() - ANNOUNCE_INTERVAL_MS * 2;
    orch.sessionUnsubscribe = () => {};

    await orch.tickPresence(false);

    expect(orch.pendingInbound).toBeNull();
    expect(patch).toHaveBeenCalledWith(expect.objectContaining({ pendingOffer: null }));
    expect(transport.sent.some((m) => m.kind === 'announce')).toBe(true);
  });
});

describe('WebTransferOrchestrator activity gate', () => {
  it('suppresses announces while link activity is not idle', async () => {
    const { orchestrator, transport } = makeOrchestrator();
    const orch = orchestrator as unknown as {
      linkActivity: string;
      sessionUnsubscribe: (() => void) | null;
      maybeSendAnnounce: (force: boolean) => Promise<void>;
    };
    orch.sessionUnsubscribe = () => {};
    orch.linkActivity = 'handshake';
    await orch.maybeSendAnnounce(true);
    expect(transport.sent.some((m) => m.kind === 'announce')).toBe(false);
  });
});
