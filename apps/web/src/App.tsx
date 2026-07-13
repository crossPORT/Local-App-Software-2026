import { useState } from 'react';
import { toDisplayPort } from '@rocketbox/sdk';
import { Header } from './components/Header';
import { ConnectionPanel } from './components/ConnectionPanel';
import { ConfirmLinkReleaseDialog } from './components/ConfirmLinkReleaseDialog';
import { EventLogDialog } from './components/EventLogDialog';
import { IncomingDialog } from './components/IncomingDialog';
import { RosterPanel } from './components/RosterPanel';
import { SettingsDialog } from './components/SettingsDialog';
import { TransferProgressPanel } from './components/TransferProgressPanel';
import { useRocketBox } from './hooks/useRocketBox';
import type { PeerEntry } from './lib/types';

/** Root RocketBox shell — peers, USB connect, transfer progress. */
export function App() {
  const [eventLogOpen, setEventLogOpen] = useState(false);
  const [releasePeer, setReleasePeer] = useState<PeerEntry | null>(null);
  const {
    state,
    settingsOpen,
    setSettingsOpen,
    ledPulse,
    usbDescription,
    disconnectUsb,
    forgetUsb,
    recoverUsb,
    saveIdentity,
    sendToPeer,
    acceptOffer,
    declineOffer,
    resetTransfer,
    requestAnnounce,
    releaseLinkedCircuit,
    patch,
  } = useRocketBox();

  if (!state) {
    return <div className="app loading">Loading RocketBox App…</div>;
  }

  return (
    <div className="app">
      <Header
        state={state}
        ledPulse={ledPulse}
        onOpenSettings={() => setSettingsOpen(true)}
        onAnnounce={requestAnnounce}
      />
      <div className="app-main">
        <ConnectionPanel state={state} usbDescription={usbDescription}>
          {!state.usbConnected ? (
            <div className="connect-actions">
              <button type="button" className="primary connect-btn" onClick={() => void recoverUsb()}>
                Connect USB
              </button>
              {state.hasSavedCable ? (
                <button type="button" className="disconnect-btn" onClick={() => void forgetUsb()}>
                  Clear saved cable
                </button>
              ) : null}
            </div>
          ) : (
            <div className="connect-actions connected-actions">
              <button
                type="button"
                className="disconnect-btn"
                onClick={() => {
                  void disconnectUsb();
                }}
              >
                Disconnect
              </button>
            </div>
          )}
        </ConnectionPanel>
        <RosterPanel
          peers={state.roster}
          fabricConnected={state.fabricConnected}
          identityConfigured={!!state.identity.display_name.trim()}
          identityDisplayName={state.identity.display_name}
          localLeg={state.portIndex}
          busy={state.busy || state.pendingOffer != null}
          statusMessage={state.statusMessage}
          selectedPeer={state.selectedPeer}
          lastAnnounceMs={state.lastAnnounceMs}
          linkedPort={state.linkedPort}
          linkState={state.linkState}
          announceIntervalSec={state.identity.announce_interval_sec}
          onSelectPeer={(name) => patch({ selectedPeer: name })}
          onDropFiles={(peerName, files) => sendToPeer(peerName, files)}
          onOpenSettings={() => setSettingsOpen(true)}
          onDropError={(message) => patch({ errorMessage: message })}
          onRequestReleaseLink={(peer) => setReleasePeer(peer)}
        />
      </div>
      <TransferProgressPanel state={state} onReset={() => void resetTransfer()} />

      {settingsOpen && !eventLogOpen && (
        <SettingsDialog
          identity={state.identity}
          onClose={() => setSettingsOpen(false)}
          onSave={saveIdentity}
          onOpenEventLog={() => setEventLogOpen(true)}
        />
      )}

      {eventLogOpen && (
        <EventLogDialog
          onClose={() => {
            setEventLogOpen(false);
            setSettingsOpen(true);
          }}
        />
      )}

      {state.pendingOffer && (
        <IncomingDialog offer={state.pendingOffer} onAccept={acceptOffer} onDecline={declineOffer} />
      )}

      {releasePeer && (
        <ConfirmLinkReleaseDialog
          peerLabel={releasePeer.display_name}
          displayPort={toDisplayPort(releasePeer.port_index)}
          waitingForAccept={state.waitingForPartner || state.busy}
          onCancel={() => setReleasePeer(null)}
          onConfirm={() => {
            setReleasePeer(null);
            void releaseLinkedCircuit();
          }}
        />
      )}
    </div>
  );
}
