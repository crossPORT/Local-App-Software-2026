import { useState } from 'react';
import { Header } from './components/Header';
import { ConnectionPanel } from './components/ConnectionPanel';
import { EventLogDialog } from './components/EventLogDialog';
import { IncomingDialog } from './components/IncomingDialog';
import { RosterPanel } from './components/RosterPanel';
import { SettingsDialog } from './components/SettingsDialog';
import { TransferProgressPanel } from './components/TransferProgressPanel';
import { useRocketBox } from './hooks/useRocketBox';

/** Root RocketBox shell — peers, USB connect, transfer progress. */
export function App() {
  const [eventLogOpen, setEventLogOpen] = useState(false);
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
        onOpenEventLog={() => setEventLogOpen(true)}
      />
      <div className="app-main">
        <ConnectionPanel state={state} usbDescription={usbDescription}>
          {!state.usbConnected ? (
            <div className="connect-actions">
              <button type="button" className="primary connect-btn" onClick={() => void recoverUsb()}>
                Connect USB
              </button>
              <button type="button" className="disconnect-btn" onClick={() => void forgetUsb()}>
                Clear saved cable
              </button>
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
          onSelectPeer={(name) => patch({ selectedPeer: name })}
          onDropFiles={(peerName, files) => sendToPeer(peerName, files)}
          onOpenSettings={() => setSettingsOpen(true)}
          onDropError={(message) => patch({ errorMessage: message })}
        />
      </div>
      <TransferProgressPanel state={state} onReset={() => void resetTransfer()} />

      {settingsOpen && (
        <SettingsDialog
          identity={state.identity}
          portIndex={state.portIndex}
          onClose={() => setSettingsOpen(false)}
          onSave={saveIdentity}
          onOpenEventLog={() => {
            setSettingsOpen(false);
            setEventLogOpen(true);
          }}
        />
      )}

      {eventLogOpen && <EventLogDialog onClose={() => setEventLogOpen(false)} />}

      {state.pendingOffer && (
        <IncomingDialog offer={state.pendingOffer} onAccept={acceptOffer} onDecline={declineOffer} />
      )}
    </div>
  );
}
