import { describe, expect, it, vi } from 'vitest';
import { ANNOUNCE_INTERVAL_MS, WebTransferOrchestrator } from './web_transfer_orchestrator';
import type { OrchestratorCallbacks } from './web_transfer_orchestrator';
import type { FabricTransport } from './fabric_transport';
import type { FabricSessionMessage } from './fabric_session';
import { PeerRoster } from './peer_roster';
import { defaultIdentityProfile } from './config';
import type { AppUiState } from './types';

function makeTransport(): FabricTransport & { sent: FabricSessionMessage[] } {
  const sent: FabricSessionMessage[] = [];
  return {
    sent,
    connected: true,
    getFabricPortIndex: () => 0,
    connect: async () => '',
    reconnectKnown: async () => '',
    describeDevice: () => '',
    disconnect: async () => {},
    forgetThisDevice: async () => {},
    resetConnection: async () => '',
    ownsDevice: () => false,
    markDisconnected: () => {},
    sendBytes: async () => {},
    receiveHeader: async () => ({ fileSize: 0, filename: '' }),
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
  it('resumes announcing after a stuck payloadSendPending flag while idle', async () => {
    const { orchestrator, transport, patch } = makeOrchestrator();
    const orch = orchestrator as unknown as {
      payloadSendPending: boolean;
      listenerPaused: boolean;
      lastAnnounceMs: number;
      tickPresence: (initial: boolean) => Promise<void>;
    };

    // Simulate a transfer that aborted without clearing its in-flight flags
    // while the station is otherwise idle (no offer/inbound/busy).
    orch.payloadSendPending = true;
    orch.listenerPaused = true;
    orch.lastAnnounceMs = Date.now() - ANNOUNCE_INTERVAL_MS * 3;

    await orch.tickPresence(false);

    expect(orch.payloadSendPending).toBe(false);
    expect(orch.listenerPaused).toBe(false);
    expect(transport.sent.some((m) => m.kind === 'announce')).toBe(true);
    expect(patch).toHaveBeenCalledWith(expect.objectContaining({ lastAnnounceMs: expect.any(Number) }));
  });

  it('does not clear handshake flags during an active outbound transfer', async () => {
    const { orchestrator, transport } = makeOrchestrator();
    const orch = orchestrator as unknown as {
      payloadSendPending: boolean;
      busy: boolean;
      tickPresence: (initial: boolean) => Promise<void>;
    };

    orch.busy = true;
    orch.payloadSendPending = true;

    await orch.tickPresence(false);

    expect(orch.payloadSendPending).toBe(true);
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

    // A previous offer left pendingInbound wedged (e.g. mobile froze the dialog timer).
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
    // Android Chrome: the Notification constructor throws "Illegal constructor".
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
      tickPresence: (initial: boolean) => Promise<void>;
    };

    orch.pendingInbound = makeOffer('wedged-session');
    orch.pendingInboundAt = Date.now() - 70_000;
    orch.lastAnnounceMs = Date.now() - ANNOUNCE_INTERVAL_MS * 2;

    await orch.tickPresence(false);

    expect(orch.pendingInbound).toBeNull();
    expect(patch).toHaveBeenCalledWith(expect.objectContaining({ pendingOffer: null }));
    expect(transport.sent.some((m) => m.kind === 'announce')).toBe(true);
  });
});
