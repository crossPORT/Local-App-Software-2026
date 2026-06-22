import { Header } from './components/Header';
import { ConnectionPanel } from './components/ConnectionPanel';
import { IncomingDialog } from './components/IncomingDialog';
import { RosterPanel } from './components/RosterPanel';
import { SettingsDialog } from './components/SettingsDialog';
import { TransferProgressPanel } from './components/TransferProgressPanel';
import { useRocketBox } from './hooks/useRocketBox';

/** Root RocketBox shell — peers, USB connect, transfer progress. */
export function App() {
  const {
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
    resetTransfer,
    patch,
  } = useRocketBox();

  if (!state) {
    return <div className="app loading">Loading RocketBox…</div>;
  }

  const onConnect = () => {
    void connectUsb();
  };

  return (
    <div className="app">
      <Header state={state} ledPulse={ledPulse} onOpenSettings={() => setSettingsOpen(true)} />
      <div className="app-main">
        <ConnectionPanel state={state} usbDescription={usbDescription}>
          {!state.usbConnected ? (
            <div className="connect-actions">
              <button type="button" className="primary connect-btn" onClick={onConnect}>
                Connect USB
              </button>
              {state.fabricDevicesSeen > 0 && (
                <button type="button" className="disconnect-btn" onClick={() => void forgetUsb()}>
                  Forget USB device
                </button>
              )}
              {state.errorMessage && (
                <button type="button" className="disconnect-btn" onClick={clearTransferState}>
                  Clear errors
                </button>
              )}
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
                Disconnect USB
              </button>
            </div>
          )}
        </ConnectionPanel>
        <RosterPanel
          peers={state.roster}
          fabricConnected={state.fabricConnected}
          identityConfigured={!!state.identity.display_name.trim()}
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
        />
      )}

      {state.pendingOffer && (
        <IncomingDialog offer={state.pendingOffer} onAccept={acceptOffer} onDecline={declineOffer} />
      )}
    </div>
  );
}
