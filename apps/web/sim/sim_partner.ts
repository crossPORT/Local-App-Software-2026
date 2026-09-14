import { defaultIdentityProfile } from '../src/lib/config';
import { PeerRoster } from '../src/lib/peer_roster';
import { WebTransferOrchestrator } from '../src/lib/web_transfer_orchestrator';
import { SIM_CABLE_SERIALS } from './fabric_hub';
import { FabricSimSession } from './fabric_sim_session';

const PARTNER_SERIAL = SIM_CABLE_SERIALS[1]!;
const PARTNER_NAME = 'Bob';

let stopFn: (() => void) | null = null;

export async function startSimPartner(): Promise<void> {
  await stopSimPartner();
  const session = new FabricSimSession();
  await session.connect(PARTNER_SERIAL, { resetHub: false, remember: false });
  const identity = {
    ...defaultIdentityProfile(session.getFabricPortIndex()),
    display_name: PARTNER_NAME,
    team: 'Sim',
    receive_status: 'open' as const,
  };
  const roster = new PeerRoster();
  const orchestrator = new WebTransferOrchestrator(session, {
    patch: () => {},
    getIdentity: () => identity,
    getPortIndex: () => session.getFabricPortIndex(),
    getRoster: () => roster,
    onUsbDescription: () => {},
    downloadPayload: () => {},
    isDisconnecting: () => false,
  });
  orchestrator.startListener();
  orchestrator.sendAnnounceNow();
  stopFn = () => {
    orchestrator.stopListener();
    void session.disconnect();
  };
}

export async function stopSimPartner(): Promise<void> {
  stopFn?.();
  stopFn = null;
}
