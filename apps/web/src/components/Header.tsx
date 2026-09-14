import { theme } from '../lib/theme';

interface HeaderProps {
  onOpenSettings: () => void;
}

export function Header({ onOpenSettings }: HeaderProps) {
  return (
    <header className="header panel" style={{ background: theme.header }}>
      <div className="title-row">
        <div className="icon-box" aria-hidden>
          <img className="title-logo" src="/rocketbox-mark.png" alt="" />
        </div>
        <div className="brand-block">
          <img className="brand-wordmark" src="/rocketbox-wordmark.png" alt="ROCKETBOX" />
          <div className="brand">Transfer</div>
        </div>
        <button type="button" className="icon-btn" onClick={onOpenSettings} title="Settings">
          ⚙
        </button>
      </div>
    </header>
  );
}
