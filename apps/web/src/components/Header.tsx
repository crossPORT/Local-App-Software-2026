import { ConnectionLed, deriveLinkLed, statusLine } from './ConnectionLed';
import { theme } from '../lib/theme';
import type { AppUiState } from '../lib/types';

interface HeaderProps {
  state: AppUiState;
  ledPulse: boolean;
  onOpenSettings: () => void;
}

export function Header({ state, ledPulse, onOpenSettings }: HeaderProps) {
  const led = deriveLinkLed(state.usbConnected, state.fabricConnected, state.busy);
  const status = statusLine(
    state.usbConnected,
    state.fabricConnected,
    state.fabricDevicesSeen,
    state.portIndex,
    state.busy,
    state.waitingForPartner,
    state.statusMessage,
    state.identity.display_name.trim() ? state.roster.length > 0 : false,
  );

  return (
    <header className="header panel" style={{ background: theme.header }}>
      <div className="title-row">
        <div className="icon-box" aria-hidden>🚀</div>
        <div className="brand-block">
          <div className="brand-row">
            <span className="brand">RocketBox</span>
            <span className="node-name">{state.identity.display_name}</span>
            <ConnectionLed state={led} pulseOn={ledPulse} />
          </div>
          <div className="status-line" style={{ color: status.colour }}>
            {status.text}
          </div>
        </div>
        <button type="button" className="icon-btn" onClick={onOpenSettings} title="Settings">
          ⚙
        </button>
      </div>
    </header>
  );
}
