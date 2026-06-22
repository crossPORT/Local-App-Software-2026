/** Rolling Mbps samples for the transfer speed monitor (heart-monitor style chart). */

export function bytesToActivityMbps(bytes: number, windowSec = 0.08): number {
  return Math.max(0.12, (bytes / (1024 * 1024)) / windowSec);
}

export class SpeedHistory {
  private samples: number[] = [];
  private lastPushMs = 0;
  private lastValue = -1;

  constructor(
    private readonly maxSamples = 150,
    private readonly minIntervalMs = 100,
  ) {}

  push(mbps: number, now = Date.now()): boolean {
    const value = Math.max(0, mbps);
    if (
      this.lastPushMs > 0 &&
      now - this.lastPushMs < this.minIntervalMs &&
      Math.abs(value - this.lastValue) < 0.05
    ) {
      return false;
    }
    this.lastPushMs = now;
    this.lastValue = value;
    this.samples.push(value);
    while (this.samples.length > this.maxSamples) {
      this.samples.shift();
    }
    return true;
  }

  clear(): void {
    this.samples = [];
    this.lastPushMs = 0;
    this.lastValue = -1;
  }

  getSamples(): readonly number[] {
    return this.samples;
  }

  /** Y-axis ceiling with headroom so the trace does not clip. */
  scaleMax(floorMbps = 8): number {
    let peak = floorMbps;
    for (const sample of this.samples) {
      if (sample > peak) {
        peak = sample;
      }
    }
    return Math.max(floorMbps, peak * 1.12);
  }
}
