import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react';
import {
  boothLog,
  clearBoothLog,
  filterBoothLogLinesByPort,
  getBoothLogLevel,
  readBoothLogLines,
  setBoothLogLevel,
  subscribeBoothLog,
  type BoothLogLevel,
} from '../lib/booth_log';
import { FABRIC_LEG_COUNT, displayPortFromLeg } from '../lib/fabric_port';
import { diagnoseBoothLog, parseBoothLogLine } from '../lib/booth_log_diagnostics';
import { theme } from '../lib/theme';

interface EventLogDialogProps {
  onClose: () => void;
}

const STICKY_THRESHOLD_PX = 48;

function eventTone(event: string): string {
  if (event.includes('fail') || event.includes('error') || event.includes('reject')) {
    return 'error';
  }
  if (
    event.includes('sent') ||
    event.includes('connect') ||
    event.includes('announce') ||
    event === 'session_sent' ||
    event === 'session_received' ||
    event === 'session_frame'
  ) {
    return 'ok';
  }
  if (event === 'usb_queue' || event === 'listenPoll') {
    return 'muted';
  }
  if (event.includes('listen') || event.includes('link_activity')) {
    return 'info';
  }
  return 'default';
}

function EventLogLine({ line }: { line: string }) {
  const parsed = parseBoothLogLine(line);
  if (!parsed) {
    return (
      <div className="event-log-line event-log-line--meta">
        <span className="event-log-detail">{line}</span>
      </div>
    );
  }
  const { time, port, event, detail } = parsed;
  const tone = eventTone(event);
  return (
    <div className={`event-log-line event-log-line--${tone}`}>
      <time className="event-log-time" dateTime={time}>
        {time.slice(11, 23)}
      </time>
      <span className="event-log-port">P{port}</span>
      <span className="event-log-event">{event}</span>
      {detail ? <span className="event-log-detail">{detail}</span> : null}
    </div>
  );
}

export function EventLogDialog({ onClose }: EventLogDialogProps) {
  const [entries, setEntries] = useState(() => readBoothLogLines());
  const [legFilter, setLegFilter] = useState<number | null>(null);
  const [level, setLevel] = useState<BoothLogLevel>(() => getBoothLogLevel());
  const [followLive, setFollowLive] = useState(true);
  const [copyState, setCopyState] = useState<'idle' | 'copied' | 'failed'>('idle');

  const scrollRef = useRef<HTMLDivElement>(null);
  const followLiveRef = useRef(true);

  const refresh = useCallback(() => {
    setEntries(readBoothLogLines());
  }, []);

  useEffect(() => {
    refresh();
    if (getBoothLogLevel() === 'off') {
      setBoothLogLevel('normal');
      setLevel('normal');
      boothLog(0, 'log_enabled', 'auto-enabled for event log viewer');
      refresh();
    }
    return subscribeBoothLog(refresh);
  }, [refresh]);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        onClose();
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [onClose]);

  const displayLines = filterBoothLogLinesByPort(entries, legFilter);
  const diagnoses = diagnoseBoothLog(entries).filter(
    (d) => legFilter === null || d.port === null || d.port === displayPortFromLeg(legFilter),
  );
  const displayText =
    displayLines.length === 0
      ? legFilter === null
        ? '(log is empty)'
        : `(no lines for port ${legFilter === null ? '?' : displayPortFromLeg(legFilter)} yet)`
      : displayLines.join('\n');

  const scrollToBottom = useCallback((behavior: ScrollBehavior = 'auto') => {
    const el = scrollRef.current;
    if (!el) {
      return;
    }
    el.scrollTo({ top: el.scrollHeight, behavior });
  }, []);

  const handleScroll = useCallback(() => {
    const el = scrollRef.current;
    if (!el) {
      return;
    }
    const distanceFromBottom = el.scrollHeight - el.scrollTop - el.clientHeight;
    const pinned = distanceFromBottom <= STICKY_THRESHOLD_PX;
    followLiveRef.current = pinned;
    setFollowLive(pinned);
  }, []);

  useLayoutEffect(() => {
    if (followLiveRef.current) {
      scrollToBottom('auto');
    }
  }, [displayLines.length, displayText, scrollToBottom]);

  useEffect(() => {
    followLiveRef.current = true;
    setFollowLive(true);
    requestAnimationFrame(() => scrollToBottom('auto'));
  }, [scrollToBottom]);

  const onLevelChange = (next: BoothLogLevel) => {
    setLevel(next);
    setBoothLogLevel(next);
    refresh();
  };

  const onCopy = async () => {
    try {
      await navigator.clipboard.writeText(`${displayText}\n`);
      setCopyState('copied');
      window.setTimeout(() => setCopyState('idle'), 1600);
    } catch {
      setCopyState('failed');
      window.setTimeout(() => setCopyState('idle'), 2000);
    }
  };

  const onDownload = () => {
    const blob = new Blob([`${displayText}\n`], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `rocketbox-event-log-${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.txt`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const onClear = () => {
    clearBoothLog();
    followLiveRef.current = true;
    setFollowLive(true);
    refresh();
    requestAnimationFrame(() => scrollToBottom('auto'));
  };

  const resumeLive = () => {
    followLiveRef.current = true;
    setFollowLive(true);
    scrollToBottom('smooth');
  };

  return (
    <div className="modal-backdrop" role="presentation" onClick={onClose}>
      <div
        className="modal event-log-dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="event-log-title"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="event-log-header">
          <div>
            <h2 id="event-log-title">Event log</h2>
            <p className="settings-hint event-log-subtitle" style={{ color: theme.muted }}>
              Live device diagnostics. Enable in Settings or with <code>?debug_log=1</code>.
            </p>
          </div>
          <button type="button" className="event-log-close" onClick={onClose} aria-label="Close">
            ×
          </button>
        </div>

        <div className="event-log-toolbar">
          <label className="event-log-field">
            Level
            <select
              value={level}
              onChange={(e) => onLevelChange(e.target.value as BoothLogLevel)}
            >
              <option value="off">Off</option>
              <option value="normal">Normal</option>
              <option value="verbose">Verbose</option>
            </select>
          </label>
          <label className="event-log-field">
            Port
            <select
              value={legFilter === null ? 'all' : String(legFilter)}
              onChange={(e) => {
                const v = e.target.value;
                setLegFilter(v === 'all' ? null : Number.parseInt(v, 10));
              }}
            >
              <option value="all">All</option>
              {Array.from({ length: FABRIC_LEG_COUNT }, (_, leg) => (
                <option key={leg} value={String(leg)}>
                  Port {displayPortFromLeg(leg)}
                </option>
              ))}
            </select>
          </label>
          <div className="event-log-actions">
            <button type="button" onClick={onCopy}>
              {copyState === 'copied' ? 'Copied' : copyState === 'failed' ? 'Copy failed' : 'Copy'}
            </button>
            <button type="button" onClick={onClear}>
              Clear
            </button>
            <button type="button" onClick={onDownload}>
              Download
            </button>
          </div>
        </div>

        <div className="event-log-status" aria-live="polite">
          <span>
            {displayLines.length} line{displayLines.length === 1 ? '' : 's'}
            {legFilter !== null ? ` · port ${displayPortFromLeg(legFilter)}` : ''}
          </span>
          <span className={followLive ? 'event-log-live event-log-live--on' : 'event-log-live'}>
            {followLive ? '● Live' : 'Paused'}
          </span>
        </div>

        {diagnoses.length > 0 && (
          <div className="event-log-diagnostics" role="status" aria-live="polite">
            {diagnoses.map((d) => (
              <div
                key={d.id}
                className={`event-log-diagnosis event-log-diagnosis--${d.severity}`}
              >
                <span className="event-log-diagnosis-title">{d.title}</span>
                <span className="event-log-diagnosis-detail">{d.detail}</span>
              </div>
            ))}
          </div>
        )}

        <div className="event-log-scroll-wrap">
          <div
            ref={scrollRef}
            className="event-log-scroll"
            onScroll={handleScroll}
            tabIndex={0}
            role="log"
            aria-label="Device event log"
          >
            {displayLines.length === 0 ? (
              <p className="event-log-empty">{displayText}</p>
            ) : (
              displayLines.map((line, index) => <EventLogLine key={`${index}-${line}`} line={line} />)
            )}
          </div>
          {!followLive && (
            <button type="button" className="event-log-jump" onClick={resumeLive}>
              Jump to latest ↓
            </button>
          )}
        </div>

        <div className="modal-actions">
          <button type="button" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
