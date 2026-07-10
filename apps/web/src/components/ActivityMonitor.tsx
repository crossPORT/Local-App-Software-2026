import { useEffect, useRef } from 'react';
import { ActivityHistory, pushCompletedTransferSpike } from '../lib/activity_history';
import { formatMbps } from '../lib/format';
import { theme } from '../lib/theme';
import { useSessionRates } from '../hooks/useSessionRates';
import { drawActivityMonitor } from './activity_monitor_draw';
import { SessionRateRow } from './SessionRateRow';

interface ActivityMonitorProps {
  visible: boolean;
  sessionPulse: number;
  transferMbps: number;
  resultMbps?: number;
  scaleFloorMbps?: number;
  compact?: boolean;
  persistHistory?: boolean;
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
  const { rates, stats } = useSessionRates(resultMbps);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const historyRef = useRef(new ActivityHistory());
  const lastSessionPulseRef = useRef(0);
  const wasVisibleRef = useRef(false);
  const prevResultMbpsRef = useRef(0);
  const lastStatsCountRef = useRef(0);
  const scaleRef = useRef(scaleFloorMbps);
  const compactRef = useRef(compact);
  scaleRef.current = scaleFloorMbps;
  compactRef.current = compact;

  const liveRef = useRef({ transferMbps, resultMbps, sessionPulse, rates, statsCount: 0 });
  liveRef.current = {
    transferMbps,
    resultMbps,
    sessionPulse,
    rates,
    statsCount: stats.count,
  };

  const ingest = () => {
    const history = historyRef.current;
    const live = liveRef.current;

    if (live.sessionPulse > lastSessionPulseRef.current) {
      const delta = live.sessionPulse - lastSessionPulseRef.current;
      for (let i = 0; i < delta; i += 1) {
        history.pushSession();
      }
      lastSessionPulseRef.current = live.sessionPulse;
    }

    if (live.transferMbps > 0) {
      history.pushTransfer(live.transferMbps);
    }

    prevResultMbpsRef.current = pushCompletedTransferSpike(
      history,
      live.resultMbps,
      prevResultMbpsRef.current,
    );
    if (live.statsCount > lastStatsCountRef.current) {
      const completed =
        live.resultMbps > 0 ? live.resultMbps : live.rates[live.rates.length - 1] ?? 0;
      if (completed > 0) {
        history.pushTransfer(completed);
      }
      lastStatsCountRef.current = live.statsCount;
    }
  };

  useEffect(() => {
    const history = historyRef.current;
    if (!visible) {
      if (!persistHistory) {
        history.clear();
        lastSessionPulseRef.current = 0;
        prevResultMbpsRef.current = 0;
        lastStatsCountRef.current = 0;
      }
      wasVisibleRef.current = false;
      return;
    }

    if (!wasVisibleRef.current && !persistHistory) {
      history.clear();
      lastSessionPulseRef.current = 0;
      prevResultMbpsRef.current = 0;
      lastStatsCountRef.current = 0;
    }
    wasVisibleRef.current = true;
    history.prime();

    let raf = 0;
    const frame = () => {
      ingest();
      const canvas = canvasRef.current;
      if (canvas) {
        drawActivityMonitor(
          canvas,
          history,
          compactRef.current,
          scaleRef.current,
          Date.now(),
        );
      }
      raf = window.requestAnimationFrame(frame);
    };
    raf = window.requestAnimationFrame(frame);
    return () => window.cancelAnimationFrame(raf);
  }, [visible, persistHistory]);

  useEffect(() => {
    if (visible) {
      ingest();
    }
  }, [visible, sessionPulse, transferMbps, resultMbps, rates, stats.count]);

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
              <span className="activity-speed-value" style={{ color: theme.transferBar }}>
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
