import type { ActivityHistory } from '../lib/activity_history';
import { theme } from '../lib/theme';

/** Draw session (blue) + transfer (teal) with sub-bucket pixel scroll. */
export function drawActivityMonitor(
  canvas: HTMLCanvasElement,
  history: ActivityHistory,
  compact: boolean,
  scaleFloorMbps: number,
  now = Date.now(),
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

  const pixelW = Math.round(width * dpr);
  const pixelH = Math.round(height * dpr);
  if (canvas.width !== pixelW || canvas.height !== pixelH) {
    canvas.width = pixelW;
    canvas.height = pixelH;
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

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

  const phase = history.scrollPhase(now);
  const buckets = history.getBuckets();
  const idleFloor = compact ? 4 : 8;
  const transferMax = history.transferScaleMax(
    scaleFloorMbps > 0 ? Math.max(idleFloor, scaleFloorMbps * 0.85) : idleFloor,
  );

  const barCount = Math.max(buckets.length, 1);
  const gap = 2;
  const barW = Math.max(2, (plotW - gap * (barCount - 1)) / barCount);
  const stride = barW + gap;
  const scrollX = phase * stride;
  const sessionMax = 5;
  const sessionCap = plotH * 0.38;
  const baseY = plotTop + plotH;

  ctx.save();
  ctx.beginPath();
  ctx.rect(padX, plotTop, plotW, plotH);
  ctx.clip();

  for (let i = 0; i < buckets.length; i += 1) {
    const bucket = buckets[i]!;
    const x = padX + i * stride - scrollX;

    if (bucket.session > 0) {
      const sessionH = Math.max(2, (bucket.session / sessionMax) * sessionCap);
      ctx.fillStyle = theme.sessionBar;
      ctx.fillRect(x, baseY - sessionH, barW, sessionH);
    }

    if (bucket.transferMbps > 0) {
      const transferH = Math.max(2, (bucket.transferMbps / transferMax) * plotH);
      ctx.fillStyle = theme.transferBar;
      ctx.fillRect(x, baseY - transferH, barW, transferH);
    }
  }
  ctx.restore();

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
