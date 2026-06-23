import { useCallback, useEffect, useRef, useState } from 'react';
import { applyBoothDisplaySettings, loadIdentityProfileAsync, saveIdentityProfile } from '../lib/config';
import { PeerRoster } from '../lib/peer_roster';
import { formatUsbConnectError } from '../lib/user_errors';
import type { AppUiState, IdentityProfile } from '../lib/types';
import { initialUiState } from '../lib/types';
import { WebTransferOrchestrator } from '../lib/web_transfer_orchestrator';
import {
  countTransportDevices,
  createTransportSession,
  clearTransportSavedPairing,
  transportHasSavedSerial,
} from '../transport_factory';
import { webUsbBlockedReason } from '../lib/webusb_env';

export function identityNeedsSetup(identity: IdentityProfile): boolean {
  return !identity.display_name.trim();
}

/** Optional URL hint for which identity profile to load — not used for USB cable selection. */
function readIdentityPortHint(): number {
  const params = new URLSearchParams(window.location.search);
  const port = Number.parseInt(params.get('port') ?? '0', 10);
  return Number.isFinite(port) && port >= 0 ? port : 0;
}

const MANUAL_DISCONNECT_KEY = 'rocketbox-manual-disconnect';
/** Ignore the sub-second disconnect blip from a transfer's resetConnection. */
const AUTO_RECOVER_MIN_DOWN_MS = 3_000;

function pickSelectedPeer(roster: PeerRoster, portIndex: number, current: string): string {
  if (current && roster.findById(current)) {
    return current;
  }
  const online = roster.visiblePeers(true);
  return online.find((peer) => peer.port_index !== portIndex)?.id ?? online[0]?.id ?? '';
}

function syncFabricPortIndex(
  session: ReturnType<typeof createTransportSession>,
  portIndexRef: { current: number },
): number {
  const fabricPort = session.getFabricPortIndex();
  portIndexRef.current = fabricPort;
  return fabricPort;
}

export function useRocketBox() {
  const identityPortHint = readIdentityPortHint();
  const portIndexRef = useRef(0);
  const sessionRef = useRef(createTransportSession());
  const rosterRef = useRef(new PeerRoster());
  const orchestratorRef = useRef<WebTransferOrchestrator | null>(null);
  const disconnectingRef = useRef(false);
  const reconnectAttemptedRef = useRef(false);
  const lastAutoRecoverMsRef = useRef(0);
  const identityRef = useRef<IdentityProfile | null>(null);

  const [state, setState] = useState<AppUiState | null>(null);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [ledPulse, setLedPulse] = useState(false);
  const [usbDescription, setUsbDescription] = useState('');
  const setupPromptedRef = useRef(false);

  const patch = useCallback((partial: Partial<AppUiState> | ((prev: AppUiState) => Partial<AppUiState>)) => {
    setState((prev) => {
      if (!prev) {
        return prev;
      }
      const next = typeof partial === 'function' ? partial(prev) : partial;
      return { ...prev, ...next };
    });
  }, []);

  identityRef.current = state?.identity ?? null;

  const downloadPayload = useCallback((data: Uint8Array, filename: string) => {
    const blob = new Blob([data.slice()], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  }, []);

  const ensureOrchestrator = useCallback((): WebTransferOrchestrator => {
    if (!orchestratorRef.current) {
      orchestratorRef.current = new WebTransferOrchestrator(sessionRef.current, {
        patch,
        getIdentity: () => identityRef.current!,
        getPortIndex: () => portIndexRef.current,
        getRoster: () => rosterRef.current,
        onUsbDescription: setUsbDescription,
        downloadPayload,
        isDisconnecting: () => disconnectingRef.current,
      });
    }
    return orchestratorRef.current;
  }, [downloadPayload, patch]);

  const clearTransferState = useCallback(() => {
    orchestratorRef.current?.clearTransferState();
    patch({
      busy: false,
      waitingForPartner: false,
      pendingOffer: null,
      errorMessage: '',
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
  }, [patch]);

  const patchError = useCallback(
    (message: string) => {
      if (disconnectingRef.current || !message) {
        return;
      }
      patch({ errorMessage: message });
    },
    [patch],
  );

  useEffect(() => {
    let cancelled = false;
    void loadIdentityProfileAsync(identityPortHint).then((identity) => {
      if (cancelled) {
        return;
      }
      rosterRef.current.seedFromConfig(identity.peers);
      setState({
        ...initialUiState(identity, identityPortHint),
        roster: rosterRef.current.visiblePeers(false),
      });
    });
    return () => {
      cancelled = true;
    };
  }, [identityPortHint]);

  useEffect(() => {
    if (!state || setupPromptedRef.current) {
      return;
    }
    if (identityNeedsSetup(state.identity)) {
      setupPromptedRef.current = true;
      setSettingsOpen(true);
    }
  }, [state]);

  useEffect(() => {
    if (!state?.busy || !state.fabricConnected) {
      setLedPulse(false);
      return;
    }
    const id = window.setInterval(() => setLedPulse((v) => !v), 500);
    return () => window.clearInterval(id);
  }, [state?.busy, state?.fabricConnected]);

  const hasStateRef = useRef(false);
  hasStateRef.current = state !== null;

  useEffect(() => {
    const onDisconnect = (event: USBConnectionEvent) => {
      if (sessionRef.current.ownsDevice(event.device)) {
        sessionRef.current.markDisconnected();
        orchestratorRef.current?.stopListener();
        // A user-initiated disconnect should drop everyone. A transient/auto
        // disconnect (cable blip the recover loop will heal) should only age out
        // stale peers — recently-seen peers stay "online" in the roster map so
        // they reappear immediately on reconnect instead of waiting ~10s for the
        // remote to re-announce.
        if (sessionStorage.getItem(MANUAL_DISCONNECT_KEY) === '1') {
          rosterRef.current.setAllPeersOffline();
        } else {
          rosterRef.current.markStalePeersOffline();
        }
        patch({
          usbConnected: false,
          fabricConnected: false,
          roster: [],
        });
        clearTransferState();
        setUsbDescription('');
      }
    };
    navigator.usb?.addEventListener('disconnect', onDisconnect);
    return () => navigator.usb?.removeEventListener('disconnect', onDisconnect);
  }, [clearTransferState, patch]);

  useEffect(() => {
    if (!state?.usbConnected || !state.fabricConnected) {
      orchestratorRef.current?.stopListener();
      return;
    }
    ensureOrchestrator().startListener();
    return () => {
      orchestratorRef.current?.stopListener();
    };
  }, [ensureOrchestrator, state?.fabricConnected, state?.usbConnected]);

  const promptSetupIfNeeded = useCallback(() => {
    const identity = identityRef.current;
    if (identity && identityNeedsSetup(identity)) {
      setSettingsOpen(true);
    }
  }, []);

  const applyUsbConnected = useCallback(
    async (desc: string) => {
      const fabricPort = syncFabricPortIndex(sessionRef.current, portIndexRef);
      const legIdentity = await loadIdentityProfileAsync(fabricPort);
      setUsbDescription(desc);
      const count = await countTransportDevices();
      patch((prev) => {
        const mergedIdentity = {
          ...prev.identity,
          display_name: prev.identity.display_name.trim() || legIdentity.display_name,
          team: prev.identity.team.trim() || legIdentity.team,
          receive_status: prev.identity.receive_status || legIdentity.receive_status,
        };
        identityRef.current = mergedIdentity;
        return {
          portIndex: fabricPort,
          identity: mergedIdentity,
          usbConnected: true,
          fabricDevicesSeen: count,
          fabricConnected: true,
          roster: rosterRef.current.visiblePeers(true),
          errorMessage: '',
        };
      });
      if (typeof Notification !== 'undefined' && Notification.permission === 'default') {
        void Notification.requestPermission();
      }
      promptSetupIfNeeded();
    },
    [patch, promptSetupIfNeeded],
  );

  const connectUsb = useCallback(async () => {
    try {
      const blocked = webUsbBlockedReason();
      if (blocked) {
        patchError(blocked);
        return;
      }
      patch({ errorMessage: '' });
      sessionStorage.removeItem(MANUAL_DISCONNECT_KEY);
      clearTransferState();
      let desc: string;
      if (transportHasSavedSerial()) {
        try {
          desc = await sessionRef.current.reconnectKnown();
        } catch {
          clearTransportSavedPairing();
          desc = await sessionRef.current.connect();
        }
      } else {
        desc = await sessionRef.current.connect();
      }
      await applyUsbConnected(desc);
    } catch (err) {
      const message = formatUsbConnectError(err);
      if (message) {
        patchError(message);
      }
    }
  }, [applyUsbConnected, clearTransferState, patch, patchError]);

  const reconnectUsb = useCallback(
    async (pickerFallback = false) => {
      try {
        const blocked = webUsbBlockedReason();
        if (blocked) {
          patchError(blocked);
          return;
        }
        orchestratorRef.current?.stopListener();
        clearTransferState();
        let desc: string;
        try {
          desc = await sessionRef.current.reconnectKnown();
        } catch (err) {
          if (!pickerFallback) {
            throw err;
          }
          clearTransportSavedPairing();
          desc = await sessionRef.current.connect();
        }
        await applyUsbConnected(desc);
      } catch (err) {
        const message = formatUsbConnectError(err);
        if (message) {
          patchError(message);
        }
      }
    },
    [applyUsbConnected, clearTransferState, patchError],
  );

  const recoverUsb = useCallback(async () => {
    await connectUsb();
  }, [connectUsb]);

  useEffect(() => {
    let staleTick = 0;
    let disconnectedSinceMs = 0;
    const tick = async () => {
      if (!hasStateRef.current) {
        return;
      }
      const count = await countTransportDevices();
      const usbConnected = sessionRef.current.connected;
      const portIndex = portIndexRef.current;
      if (usbConnected) {
        disconnectedSinceMs = 0;
        staleTick += 1;
        if (staleTick >= 15) {
          rosterRef.current.markStalePeersOffline();
          staleTick = 0;
        }
      } else {
        // A transfer ends with a deliberate disconnect→reconnect (resetConnection)
        // that briefly reports `connected === false`. Do NOT auto-recover on that
        // transient blip — a competing reconnect causes interface-claim contention.
        // Only recover once the link has stayed down for a sustained window.
        if (disconnectedSinceMs === 0) {
          disconnectedSinceMs = Date.now();
        }
        if (
          Date.now() - disconnectedSinceMs >= AUTO_RECOVER_MIN_DOWN_MS &&
          count > 0 &&
          transportHasSavedSerial() &&
          sessionStorage.getItem(MANUAL_DISCONNECT_KEY) !== '1' &&
          Date.now() - lastAutoRecoverMsRef.current >= 12_000
        ) {
          lastAutoRecoverMsRef.current = Date.now();
          void reconnectUsb();
        }
      }
      patch((prev) => ({
        usbConnected,
        fabricDevicesSeen: count,
        fabricConnected: usbConnected,
        roster: rosterRef.current.visiblePeers(usbConnected),
        selectedPeer: pickSelectedPeer(rosterRef.current, portIndex, prev.selectedPeer),
      }));
    };
    tick();
    const id = window.setInterval(tick, 1000);
    return () => window.clearInterval(id);
  }, [patch, reconnectUsb]);

  useEffect(() => {
    if (!state || state.usbConnected || reconnectAttemptedRef.current) {
      return;
    }
    if (sessionStorage.getItem(MANUAL_DISCONNECT_KEY) === '1') {
      reconnectAttemptedRef.current = true;
      return;
    }
    if (!transportHasSavedSerial()) {
      reconnectAttemptedRef.current = true;
      return;
    }
    reconnectAttemptedRef.current = true;
    void reconnectUsb();
  }, [state, state?.usbConnected, reconnectUsb]);

  const disconnectUsb = useCallback(async () => {
    disconnectingRef.current = true;
    sessionStorage.setItem(MANUAL_DISCONNECT_KEY, '1');
    orchestratorRef.current?.stopListener();
    try {
      await sessionRef.current.disconnect();
    } catch (err) {
      patchError(formatUsbConnectError(err) ?? 'Disconnect failed');
    } finally {
      clearTransferState();
      rosterRef.current.setAllPeersOffline();
      patch({
        usbConnected: false,
        fabricConnected: false,
        roster: [],
      });
      setUsbDescription('');
      disconnectingRef.current = false;
    }
  }, [clearTransferState, patch, patchError]);

  const forgetUsb = useCallback(async () => {
    disconnectingRef.current = true;
    orchestratorRef.current?.stopListener();
    sessionStorage.setItem(MANUAL_DISCONNECT_KEY, '1');
    try {
      await sessionRef.current.forgetThisDevice();
    } catch (err) {
      patchError(formatUsbConnectError(err) ?? 'Disconnect failed');
    } finally {
      clearTransferState();
      rosterRef.current.setAllPeersOffline();
      patch({
        usbConnected: false,
        fabricConnected: false,
        fabricDevicesSeen: 0,
        roster: [],
      });
      setUsbDescription('');
      disconnectingRef.current = false;
    }
  }, [clearTransferState, patch, patchError]);

  const saveIdentity = useCallback(
    (identity: IdentityProfile) => {
      const storagePort = portIndexRef.current;
      saveIdentityProfile(storagePort, identity);
      const applied = applyBoothDisplaySettings(identity, storagePort);
      identityRef.current = applied;
      rosterRef.current.seedFromConfig(applied.peers);
      const portIndex = portIndexRef.current;
      patch({
        identity: applied,
        roster: rosterRef.current.visiblePeers(state?.usbConnected ?? false),
        selectedPeer: pickSelectedPeer(rosterRef.current, portIndex, state?.selectedPeer ?? ''),
      });
      setSettingsOpen(false);
      if (state?.usbConnected) {
        ensureOrchestrator().sendAnnounceNow();
      }
    },
    [ensureOrchestrator, patch, state?.selectedPeer, state?.usbConnected],
  );

  const sendToPeer = useCallback(
    async (peerId: string, files: File[]) => {
      const file = files[0];
      if (!file) {
        patch({ errorMessage: 'No file selected' });
        return;
      }
      try {
        await ensureOrchestrator().sendToPeer(peerId, file);
      } catch (err) {
        patch({ errorMessage: err instanceof Error ? err.message : 'Send failed' });
      }
    },
    [ensureOrchestrator, patch],
  );

  const acceptOffer = useCallback(() => {
    void ensureOrchestrator().acceptPendingOffer();
  }, [ensureOrchestrator]);

  const declineOffer = useCallback(() => {
    void ensureOrchestrator().declinePendingOffer();
  }, [ensureOrchestrator]);

  const resetTransfer = useCallback(async () => {
    clearTransferState();
    await ensureOrchestrator().resetConnection();
  }, [clearTransferState, ensureOrchestrator]);

  return {
    state,
    settingsOpen,
    setSettingsOpen,
    ledPulse,
    usbDescription,
    connectUsb,
    disconnectUsb,
    forgetUsb,
    clearTransferState,
    saveIdentity,
    sendToPeer,
    acceptOffer,
    declineOffer,
    recoverUsb,
    resetTransfer,
    patch,
  };
}
