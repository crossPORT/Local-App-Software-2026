import { formatBytes, formatTransferLiveMessage, isOutboundHandshakeWait, isTransferCompleteMessage } from '../lib/format';
import { theme } from '../lib/theme';
import type { AppUiState, TransferPhase } from '../lib/types';
import { ProgressBar } from './ProgressBar';

function transferIsSending(statusMessage: string): boolean {
  return statusMessage.startsWith('Sending ');
}

function transferIsReceiving(statusMessage: string): boolean {
  return statusMessage.startsWith('Receiving ') || statusMessage.startsWith('Waiting for ');
}

function derivePhase(state: AppUiState): TransferPhase {
  if (!state.fabricConnected && state.errorMessage) {
    return 'idle';
  }
  if (state.errorMessage && !state.busy && !state.waitingForPartner && state.fabricConnected) {
    return 'failed';
  }
  if (state.pendingOffer || (state.waitingForPartner && !state.busy)) {
    return 'waiting';
  }
  if (state.busy && state.waitingForPartner) {
    return 'waiting';
  }
  if (state.busy && state.bytesTotal > 0) {
    return 'transferring';
  }
  if (!state.busy && isTransferCompleteMessage(state.notification || state.statusMessage)) {
    return 'complete';
  }
  return 'idle';
}

function phaseTitle(
  phase: TransferPhase,
  fabricConnected: boolean,
  waitingForPartner: boolean,
  statusMessage: string,
  hasPeers: boolean,
  errorMessage: string,
): string {
  switch (phase) {
    case 'idle':
      if (!fabricConnected) return 'Not connected';
      return hasPeers ? 'Ready' : 'Waiting for peers…';
    case 'waiting':
      if (isOutboundHandshakeWait(statusMessage)) {
        return 'Waiting for partner…';
      }
      return waitingForPartner ? 'Listening for files…' : 'Incoming file';
    case 'transferring':
      return statusMessage.includes('Receiving') ? 'Receiving file…' : 'Sending file…';
    case 'complete':
      return 'Done';
    case 'failed':
      return errorMessage || 'Transfer failed';
    default:
      return hasPeers ? 'Ready' : 'Waiting for peers…';
  }
}

function phaseColour(phase: TransferPhase): string {
  switch (phase) {
    case 'waiting':
      return theme.warn;
    case 'transferring':
      return theme.accent;
    case 'complete':
      return theme.ok;
    case 'failed':
      return theme.warn;
    default:
      return theme.muted;
  }
}


interface TransferProgressPanelProps {
  state: AppUiState;
  onReset?: () => void;
}

export function TransferProgressPanel({ state, onReset }: TransferProgressPanelProps) {
  const phase = derivePhase(state);
  const colour = phaseColour(phase);
  const autoReceive = state.identity.receive_status === 'open';
  const isListening =
    phase === 'waiting' && state.waitingForPartner && !state.pendingOffer && !state.busy;
  const peerName =
    state.roster.find((peer) => peer.id === state.selectedPeer)?.display_name
    ?? state.roster[0]?.display_name
    ?? '';
  const showProgress = phase === 'transferring' || phase === 'complete';
  const fraction =
    state.bytesTotal > 0 ? Math.min(1, state.bytesDone / state.bytesTotal) : 0;

  let bytesLine = '';
  let speedLine = '';
  let messageLine = state.notification || state.statusMessage;

  if (phase === 'transferring') {
    bytesLine =
      state.transferLabel && state.bytesTotal
        ? `${state.transferLabel} · ${formatBytes(state.bytesDone)} of ${formatBytes(state.bytesTotal)}`
        : `${formatBytes(state.bytesDone)} of ${formatBytes(state.bytesTotal)}`;
    const sending = transferIsSending(state.statusMessage);
    const receiving = transferIsReceiving(state.statusMessage);
    speedLine = formatTransferLiveMessage(
      sending || !receiving,
      state.liveMbps,
    );
    messageLine = '';
  } else if (phase === 'complete') {
    bytesLine = state.transferLabel
      ? `${state.transferLabel} · ${formatBytes(state.bytesTotal)}`
      : formatBytes(state.bytesTotal);
    messageLine = state.notification || state.statusMessage;
  } else if (phase === 'waiting') {
    bytesLine = state.bytesTotal ? formatBytes(state.bytesTotal) : '';
    messageLine = state.statusMessage || 'Waiting for a file…';
  } else if (phase === 'failed') {
    messageLine = '';
  } else if (phase === 'idle' && !state.fabricConnected && state.errorMessage) {
    messageLine = state.errorMessage;
  } else if (phase === 'idle' && state.fabricConnected) {
    messageLine = state.notification || '';
  } else if (phase === 'idle') {
    messageLine = state.notification || '';
  }

  return (
    <section
      className={`transfer${phase === 'idle' || phase === 'waiting' ? ' transfer--idle' : ''}${showProgress ? ' transfer--active' : ''}`}
    >
      <div className="transfer-head">
        <div className="status-label">Status</div>
        <div className="phase-title" style={{ color: isListening ? theme.muted : colour }}>
          {phaseTitle(
            phase,
            state.fabricConnected,
            state.waitingForPartner,
            state.statusMessage,
            state.roster.length > 0,
            state.errorMessage,
          )}
        </div>
      </div>
      {isListening && (
        <div className="listening-hint" style={{ color: theme.muted }}>
          {autoReceive ? (
            <>Drop a file on {peerName ?? 'a peer'} above to send. Incoming files save to Downloads.</>
          ) : (
            <>Listening for incoming files — you will be asked to save or decline.</>
          )}
        </div>
      )}
      {showProgress && (
        <div className="progress-row">
          <ProgressBar
            fraction={fraction}
            pulse={false}
            colour={colour}
          />
          <span className="percent" style={{ color: theme.muted }}>
            {Math.round(fraction * 100)}%
          </span>
        </div>
      )}
      {bytesLine && (
        <div className="bytes-line" style={{ color: theme.text }}>
          {bytesLine}
        </div>
      )}
      {speedLine && (
        <div className="speed-line" style={{ color: theme.accent }}>
          {speedLine}
        </div>
      )}
      {messageLine && !isListening && (
        <div
          className="message-line"
          style={{
            color:
              phase === 'complete'
                ? theme.ok
                : !state.fabricConnected && state.errorMessage
                  ? theme.warn
                  : theme.muted,
          }}
        >
          {messageLine}
        </div>
      )}
      {phase === 'failed' && onReset && (
        <div className="reset-row">
          <button type="button" className="reset-btn" onClick={onReset}>
            Reset connection
          </button>
        </div>
      )}
    </section>
  );
}
