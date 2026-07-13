interface ConfirmLinkReleaseDialogProps {
  peerLabel: string;
  displayPort: number;
  waitingForAccept: boolean;
  onConfirm: () => void;
  onCancel: () => void;
}

/** Confirm clearing EP4 switch to a peer (PWA; native has matching dialog). */
export function ConfirmLinkReleaseDialog({
  peerLabel,
  displayPort,
  waitingForAccept,
  onConfirm,
  onCancel,
}: ConfirmLinkReleaseDialogProps) {
  return (
    <div className="modal-backdrop" role="presentation" onClick={onCancel}>
      <div
        className="modal link-release-dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="link-release-title"
        onClick={(e) => e.stopPropagation()}
      >
        <h2 id="link-release-title">Release link?</h2>
        <p>
          Clear the switch to {peerLabel} (port {displayPort}).
        </p>
        {waitingForAccept ? (
          <p className="link-release-warn">This cancels the pending offer wait.</p>
        ) : (
          <p className="muted">You can announce again anytime to rediscover peers.</p>
        )}
        <div className="modal-actions">
          <button type="button" className="btn-decline" onClick={onCancel}>
            Keep link
          </button>
          <button type="button" className="btn-accept" onClick={onConfirm}>
            Release
          </button>
        </div>
      </div>
    </div>
  );
}
