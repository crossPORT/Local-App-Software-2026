import { theme } from '../lib/theme';

interface ProgressBarProps {
  fraction?: number;
  pulse?: boolean;
  colour?: string;
}

export function ProgressBar({ fraction = 0, pulse = false, colour = theme.accent }: ProgressBarProps) {
  return (
    <div className="progress-bar" style={{ background: theme.track }} role="progressbar">
      <div
        className={`progress-fill${pulse ? ' pulse' : ''}`}
        style={{
          background: colour,
          width: pulse ? undefined : `${Math.max(fraction * 100, fraction > 0 ? 8 : 0)}%`,
        }}
      />
    </div>
  );
}
