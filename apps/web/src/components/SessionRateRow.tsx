import { formatMbps } from '../lib/format';
import { theme } from '../lib/theme';

export function SessionRateRow({
  median,
  max,
  average,
  count,
  compact,
}: {
  median: number;
  max: number;
  average: number;
  count: number;
  compact: boolean;
}) {
  const cells = [
    { label: 'median', value: median },
    { label: 'max', value: max },
    { label: 'avg', value: average },
  ];
  return (
    <div
      className={`activity-rate-stats${compact ? ' activity-rate-stats--compact' : ''}`}
      aria-label={`Session transfer rates, ${count} transfers`}
    >
      {cells.map((cell) => (
        <div key={cell.label} className="activity-rate-cell">
          <span className="activity-rate-value" style={{ color: theme.text }}>
            {formatMbps(cell.value)}
          </span>
          <span className="activity-rate-label" style={{ color: theme.muted }}>
            {cell.label}
          </span>
        </div>
      ))}
    </div>
  );
}
