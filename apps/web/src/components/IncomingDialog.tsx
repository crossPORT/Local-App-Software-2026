import { useEffect } from 'react';
import { formatBytes } from '../lib/format';
import { theme } from '../lib/theme';
import type { PendingOffer } from '../lib/types';
import { ACCEPT_DIALOG_SEC } from '../lib/usb_constants';

interface IncomingDialogProps {
  offer: PendingOffer;
  onAccept: () => void;
  onDecline: () => void;
}

export function IncomingDialog({ offer, onAccept, onDecline }: IncomingDialogProps) {
  useEffect(() => {
    const timer = window.setTimeout(onDecline, ACCEPT_DIALOG_SEC * 1000);
    return () => window.clearTimeout(timer);
  }, [offer.payload_name, offer.total_bytes, onDecline]);

  return (
    <div className="modal-backdrop" role="presentation">
      <div className="modal incoming-dialog" role="dialog" aria-modal="true">
        <h2>Incoming file</h2>
        <p>
          From {offer.from_name}
          {offer.team ? ` (${offer.team})` : ''}
        </p>
        <p>File: {offer.payload_name}</p>
        <p>
          Size: {formatBytes(offer.total_bytes)}
          {offer.file_count > 1 ? ` (${offer.file_count} files)` : ''}
        </p>
        {offer.note && <p>Note: {offer.note}</p>}
        <p className="countdown" style={{ color: theme.muted }}>
          Declines automatically in {ACCEPT_DIALOG_SEC}s if you do nothing
        </p>
        <div className="modal-actions">
          <button type="button" className="btn-decline" onClick={onDecline}>
            Decline
          </button>
          <button type="button" className="btn-accept" onClick={onAccept}>
            Save file
          </button>
        </div>
      </div>
    </div>
  );
}
