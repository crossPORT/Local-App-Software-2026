import { useEffect, useRef } from 'react';
import { ActivityHistory } from '../lib/activity_history';
import { formatMbps } from '../lib/format';
import { theme } from '../lib/theme';
import { useSessionRates } from '../hooks/useSessionRates';

interface ActivityMonitorProps {
  visible: boolean;
  sessionPulse: number;
  transferMbps: number;
  /** Latest completed-transfer rate; used to accumulate session median/max/avg. */
  resultMbps?: number;
  scaleFloorMbps?: number;
  compact?: boolean;
  persistHistory?: boolean;
}

function drawActivityMonitor(
  canvas: HTMLCanvasElement,
  history: ActivityHistory,
  compact: boolean,
  scaleFloorMbps: number,
): void {
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    return;
  }

  const dpr = window.devicePixelRatio || 1;
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (width <= 0 || height <= 0) {
    return;
  }

  canvas.width = Math.round(width * dpr);
  canvas.height = Math.round(height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);

  const padX = 8;
  const padY = compact ? 3 : 4;
  const legendFontPx = compact ? 11 : 12;
  const legendGap = compact ? 10 : 14;
  const legendH = legendGap + legendFontPx + 6;
  const plotW = width - padX * 2;
  const plotH = height - padY * 2 - legendH;
  const plotTop = padY;

  ctx.fillStyle = theme.monitorBg;
  ctx.fillRect(0, 0, width, height);

  const buckets = history.getBuckets();
  const idleFloor = compact ? 4 : 8;
  const transferMax = history.transferScaleMax(
    scaleFloorMbps > 0 ? Math.max(idleFloor, scaleFloorMbps * 0.85) : idleFloor,
  );

  const barCount = Math.max(buckets.length, 1);
  const gap = 2;
  const barW = Math.max(2, (plotW - gap * (barCount - 1)) / barCount);
  const sessionMax = 5;
  const sessionCap = plotH * 0.38;

  for (let i = 0; i < buckets.length; i += 1) {
    const bucket = buckets[i];
    const x = padX + i * (barW + gap);
    const baseY = plotTop + plotH;

    if (bucket.session > 0) {
      const sessionH = (bucket.session / sessionMax) * sessionCap;
      ctx.fillStyle = theme.sessionBar;
      ctx.fillRect(x, baseY - sessionH, barW, sessionH);
    }

    if (bucket.transferMbps > 0) {
      const transferH = (bucket.transferMbps / transferMax) * plotH;
      ctx.fillStyle = theme.transferBar;
      ctx.fillRect(x, baseY - transferH, barW, transferH);
    }
  }

  const legendY = plotTop + plotH + legendGap + legendFontPx * 0.5;
  const swatch = compact ? 8 : 9;
  ctx.font = `600 ${legendFontPx}px system-ui, sans-serif`;
  ctx.textBaseline = 'middle';

  ctx.fillStyle = theme.sessionBar;
  ctx.fillRect(padX, legendY - swatch / 2, swatch, swatch);
  ctx.fillStyle = theme.muted;
  ctx.fillText('session', padX + swatch + 6, legendY);

  const transferX = padX + swatch + 6 + ctx.measureText('session').width + 14;
  ctx.fillStyle = theme.transferBar;
  ctx.fillRect(transferX, legendY - swatch / 2, swatch, swatch);
  ctx.fillStyle = theme.muted;
  ctx.fillText('transfer', transferX + swatch + 6, legendY);
}

function SessionRateRow({
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

export function ActivityMonitor({
  visible,
  sessionPulse,
  transferMbps,
  resultMbps = 0,
  scaleFloorMbps = 0,
  compact = false,
  persistHistory = false,
}: ActivityMonitorProps) {
  const { stats } = useSessionRates(resultMbps);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const historyRef = useRef(new ActivityHistory());
  const lastSessionPulseRef = useRef(0);
  const wasVisibleRef = useRef(false);
  const hadTransferRef = useRef(false);

  const redraw = () => {
    const canvas = canvasRef.current;
    if (!canvas) {
      return;
    }
    drawActivityMonitor(canvas, historyRef.current, compact, scaleFloorMbps);
  };

  useEffect(() => {
    const history = historyRef.current;
    if (!visible) {
      if (!persistHistory) {
        history.clear();
        lastSessionPulseRef.current = 0;
      }
      wasVisibleRef.current = false;
      return;
    }

    if (!wasVisibleRef.current && !persistHistory) {
      history.clear();
      lastSessionPulseRef.current = 0;
    }
    wasVisibleRef.current = true;

    if (sessionPulse > lastSessionPulseRef.current) {
      const delta = sessionPulse - lastSessionPulseRef.current;
      for (let i = 0; i < delta; i += 1) {
        history.pushSession();
      }
      lastSessionPulseRef.current = sessionPulse;
    }

    if (transferMbps > 0) {
      history.pushTransfer(transferMbps);
      hadTransferRef.current = true;
    } else if (hadTransferRef.current) {
      history.clearTransferTrack();
      hadTransferRef.current = false;
    }

    redraw();
  }, [visible, sessionPulse, transferMbps, scaleFloorMbps, compact, persistHistory]);

  useEffect(() => {
    const onResize = () => {
      if (visible) {
        redraw();
      }
    };
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, [visible, compact, scaleFloorMbps]);

  if (!visible) {
    return null;
  }

  const showLive = transferMbps > 0;
  const showSessionStats = stats.count > 0;

  return (
    <div className={`activity-monitor${compact ? ' activity-monitor--compact' : ''}`}>
      {(showLive || showSessionStats) && (
        <div className={`activity-monitor-head${showLive ? ' activity-monitor-head--live' : ''}`}>
          {showLive ? (
            <div className="activity-monitor-live">
              <span className="activity-speed-value" style={{ color: theme.accent }}>
                {formatMbps(transferMbps)}
              </span>
              <span className="activity-speed-label" style={{ color: theme.muted }}>
                live
              </span>
            </div>
          ) : (
            <span className="activity-session-count" style={{ color: theme.muted }}>
              {stats.count} {stats.count === 1 ? 'transfer' : 'transfers'}
            </span>
          )}
          {showSessionStats && (
            <SessionRateRow
              median={stats.median}
              max={stats.max}
              average={stats.average}
              count={stats.count}
              compact={compact}
            />
          )}
        </div>
      )}
      <canvas ref={canvasRef} className="activity-monitor-canvas" />
    </div>
  );
}
