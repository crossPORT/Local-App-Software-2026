import { RocketBox } from './rocketbox';
import type { Session } from './session_core';
import type { Transport } from './transport';
import { SimTransport } from './transports/sim';
import { debugLog } from './debug_log';
import { RocketBoxError } from './errors';
import { toPortIndex } from './port';

export type AttachSessionOptions = {
  port: number;
};

function wrapAttachError(displayPort: number, err: unknown): Error {
  const msg = err instanceof Error ? err.message : String(err);
  debugLog(toPortIndex(displayPort), 'attach_fail', msg);
  if (msg === 'timeout' || /timeout/i.test(msg)) {
    return new RocketBoxError(
      'Sim connect timed out — is simulated-hardware running on :1773?',
      'timeout',
    );
  }
  return err instanceof Error ? err : new Error(msg);
}

/** Open SimTransport + Session for Port N (WS ?port=N; no ATTACH). */
export async function attachSession(
  options: AttachSessionOptions,
): Promise<{ session: Session; transport: Transport }> {
  const port = options.port;
  const transport: Transport = new SimTransport(port);
  const logLeg = toPortIndex(port);
  debugLog(logLeg, 'attach_begin', 'sim');
  let session: Session;
  try {
    session = await RocketBox.attach(transport, port);
  } catch (err) {
    try {
      await transport.disconnect();
    } catch {
      /* best effort */
    }
    throw wrapAttachError(port, err);
  }
  debugLog(logLeg, 'attach_ok', session.systemId);
  return { session, transport };
}

export async function countSimDevices(): Promise<number> {
  return 4;
}
