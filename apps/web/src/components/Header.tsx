import { BrandLockup } from './BrandLockup';
import { theme } from '../lib/theme';

interface HeaderProps {
  onOpenSettings: () => void;
}

export function Header({ onOpenSettings }: HeaderProps) {
  return (
    <header className="header panel" style={{ background: theme.header }}>
      <div className="title-row">
        <BrandLockup />
        <button type="button" className="icon-btn" onClick={onOpenSettings} title="Settings">
          ⚙
        </button>
      </div>
    </header>
  );
}
