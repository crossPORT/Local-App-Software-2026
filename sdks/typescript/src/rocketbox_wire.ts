import type { Connection } from './connection';
import type { SessionMessage } from './session_types';
import type { CircuitDataBuffer } from './rocketbox_buffer';
import {
  isLengthPrefixedFrame,
  parseIncomingSession,
} from './rocketbox_io';

export type ConnHooks = {
  unsubMsg: (() => void) | null;
  unsubData: (() => void) | null;
  unsubClosed: (() => void) | null;
};

export function bindCircuitConnection(
  conn: Connection,
  dataBuf: CircuitDataBuffer,
  sessionHandlers: Set<(m: SessionMessage) => void>,
  hooks: ConnHooks,
  onClosed: () => void,
): void {
  hooks.unsubMsg?.();
  hooks.unsubData?.();
  hooks.unsubClosed?.();
  dataBuf.clear();
  hooks.unsubMsg = conn.onMessageReceived((bytes) => {
    const msg = parseIncomingSession(bytes);
    if (msg) sessionHandlers.forEach((h) => h(msg));
  });
  hooks.unsubData = conn.onReceived((bytes) => {
    if (isLengthPrefixedFrame(bytes)) return;
    dataBuf.append(bytes);
  });
  hooks.unsubClosed = conn.onClosed(() => onClosed());
}

export function clearConnHooks(hooks: ConnHooks): void {
  hooks.unsubMsg?.();
  hooks.unsubData?.();
  hooks.unsubClosed?.();
  hooks.unsubMsg = hooks.unsubData = hooks.unsubClosed = null;
}
