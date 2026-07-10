import type { Connection } from '@rocketbox/sdk';
import type { FabricSessionMessage } from './fabric_session';
import type { CircuitDataBuffer } from './sdk_fabric_buffer';
import {
  isLengthPrefixedFrame,
  parseIncomingSession,
} from './sdk_fabric_io';

export type ConnHooks = {
  unsubMsg: (() => void) | null;
  unsubData: (() => void) | null;
  unsubClosed: (() => void) | null;
};

export function bindCircuitConnection(
  conn: Connection,
  dataBuf: CircuitDataBuffer,
  sessionHandlers: Set<(m: FabricSessionMessage) => void>,
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
