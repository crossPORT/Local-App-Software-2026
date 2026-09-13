import { boothLog } from './debug_log';
import { INTERFACE_NUMBER } from './protocol';
import { ensureInterfaceReady } from './usb_claim';
import {
  ACTIVE_IN_POLL_WAIT_MS,
  IN_SETTLE_MS,
  INTERFACE_RECOVERY_TIMEOUT_MS,
} from './link_types';
import { sleep } from './usb_util';
import type { FabricLink } from './fabric_link';

export function beginOutboundTransfer(link: FabricLink): void {
  link.listenSuspendedForSend += 1;
  link.stopListenLoop();
}

export async function prepareOutboundBus(link: FabricLink): Promise<void> {
  boothLog(link.fabricLeg, 'session_send_prep', 'wait_in');
  await Promise.race([link.activeInPoll, sleep(ACTIVE_IN_POLL_WAIT_MS)]);
  boothLog(link.fabricLeg, 'session_send_prep', 'abort_iface');
  await abortStuckTransferWithTimeout(link);
  await sleep(IN_SETTLE_MS);
  boothLog(link.fabricLeg, 'session_send_prep', 'ready');
}

export function endOutboundTransfer(link: FabricLink): void {
  link.listenSuspendedForSend = Math.max(0, link.listenSuspendedForSend - 1);
  if (link.listenMode !== 'off') {
    boothLog(link.fabricLeg, 'listen_restart', link.listenMode);
    link.startListenLoop();
  }
}

export async function abortStuckTransfer(link: FabricLink): Promise<void> {
  const device = link.getDevice();
  if (!device?.opened) {
    return;
  }
  const recovery = link.interfaceRecovery.then(
    () => runInterfaceRecovery(link),
    () => runInterfaceRecovery(link),
  );
  link.interfaceRecovery = recovery.then(
    () => undefined,
    () => undefined,
  );
  await recovery;
}

async function runInterfaceRecovery(link: FabricLink): Promise<void> {
  const current = link.getDevice();
  if (!current?.opened) {
    return;
  }
  link.clearPendingHeaderRead();
  try {
    await current.releaseInterface(INTERFACE_NUMBER);
  } catch {
    /* best effort */
  }
  await ensureInterfaceReady(current);
}

export function abortStuckTransferWithTimeout(link: FabricLink): Promise<void> {
  return Promise.race([abortStuckTransfer(link), sleep(INTERFACE_RECOVERY_TIMEOUT_MS)]).then(
    () => undefined,
  );
}
