import { useEffect, useRef } from 'react';
import { SpeedHistory } from '../lib/speed_history';
import { theme } from '../lib/theme';

interface SpeedMonitorProps {
  visible: boolean;
  activityMbps: number;
  colour?: string;
  compact?: boolean;
  persistHistory?: boolean;
}

function drawMonitor(
  canvas: HTMLCanvasElement,
  samples: readonly number[],
  scaleMax: number,
  colour: string,
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

  const padX = 6;
  const padY = 4;
  const plotW = width - padX * 2;
  const plotH = height - padY * 2;

  ctx.fillStyle = theme.monitorBg;
  ctx.fillRect(0, 0, width, height);

  ctx.strokeStyle = theme.monitorGrid;
  ctx.lineWidth = 1;
  for (let i = 1; i < 3; ++i) {
    const y = padY + (plotH * i) / 3;
    ctx.beginPath();
    ctx.moveTo(padX, y);
    ctx.lineTo(padX + plotW, y);
    ctx.stroke();
  }
  const gridStep = 24;
  for (let x = padX; x <= padX + plotW; x += gridStep) {
    ctx.beginPath();
    ctx.moveTo(x, padY);
    ctx.lineTo(x, padY + plotH);
    ctx.stroke();
  }

  if (samples.length < 2) {
    return;
  }

  ctx.strokeStyle = colour;
  ctx.lineWidth = 1.5;
  ctx.lineJoin = 'round';
  ctx.lineCap = 'round';
  ctx.shadowColor = colour;
  ctx.shadowBlur = 4;
  ctx.beginPath();

  const stepX = plotW / Math.max(samples.length - 1, 1);
  for (let i = 0; i < samples.length; ++i) {
    const x = padX + i * stepX;
    const norm = samples[i] / scaleMax;
    const y = padY + plotH - norm * plotH;
    if (i === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  }
  ctx.stroke();
  ctx.shadowBlur = 0;

  const last = samples[samples.length - 1];
  const dotX = padX + (samples.length - 1) * stepX;
  const dotY = padY + plotH - (last / scaleMax) * plotH;
  ctx.fillStyle = colour;
  ctx.beginPath();
  ctx.arc(dotX, dotY, 2.5, 0, Math.PI * 2);
  ctx.fill();
}

export function SpeedMonitor({
  visible,
  activityMbps,
  colour = theme.accent,
  compact = false,
  persistHistory = false,
}: SpeedMonitorProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const historyRef = useRef(new SpeedHistory());
  const wasVisibleRef = useRef(false);

  const redraw = () => {
    const canvas = canvasRef.current;
    if (!canvas) {
      return;
    }
    const history = historyRef.current;
    drawMonitor(canvas, history.getSamples(), history.scaleMax(compact ? 4 : 8), colour);
  };

  useEffect(() => {
    const history = historyRef.current;
    if (!visible) {
      if (!persistHistory) {
        history.clear();
      }
      wasVisibleRef.current = false;
      return;
    }

    if (!wasVisibleRef.current && !persistHistory) {
      history.clear();
    }
    wasVisibleRef.current = true;
    history.push(activityMbps);
    redraw();
  }, [visible, activityMbps, colour, compact, persistHistory]);

  useEffect(() => {
    const onResize = () => {
      if (visible) {
        redraw();
      }
    };
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, [visible, colour, compact]);

  if (!visible) {
    return null;
  }

  return (
    <div className={`speed-monitor${compact ? ' speed-monitor--compact' : ''}`}>
      <canvas ref={canvasRef} className="speed-monitor-canvas" />
    </div>
  );
}
