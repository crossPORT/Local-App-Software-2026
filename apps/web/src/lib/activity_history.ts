/** Time-bucketed USB activity with fractional scroll for smooth strip motion. */

export interface ActivityBucket {
  session: number;
  transferMbps: number;
}

/** ~18s window at 250ms columns. */
export const ACTIVITY_MAX_BUCKETS = 72;
export const ACTIVITY_BUCKET_MS = 250;

export class ActivityHistory {
  private buckets: ActivityBucket[] = [];
  private bucketStartMs = 0;

  constructor(
    private readonly maxBuckets = ACTIVITY_MAX_BUCKETS,
    private readonly bucketMs = ACTIVITY_BUCKET_MS,
    private readonly maxSessionPerBucket = 5,
  ) {}

  prime(now = Date.now()): void {
    if (this.buckets.length > 0) {
      return;
    }
    this.buckets = Array.from({ length: this.maxBuckets }, () => ({
      session: 0,
      transferMbps: 0,
    }));
    this.bucketStartMs = now;
  }

  /** Commit elapsed whole buckets; returns 0..1 phase within the current column. */
  scrollPhase(now = Date.now()): number {
    this.ensureBucket(now);
    if (this.bucketMs <= 0) {
      return 0;
    }
    return Math.min(0.999, Math.max(0, (now - this.bucketStartMs) / this.bucketMs));
  }

  advance(now = Date.now()): number {
    return this.scrollPhase(now);
  }

  private ensureBucket(now: number): void {
    if (this.buckets.length === 0) {
      this.prime(now);
      return;
    }
    while (now - this.bucketStartMs >= this.bucketMs) {
      this.bucketStartMs += this.bucketMs;
      this.buckets.push({ session: 0, transferMbps: 0 });
      while (this.buckets.length > this.maxBuckets) {
        this.buckets.shift();
      }
    }
  }

  pushSession(now = Date.now()): void {
    this.ensureBucket(now);
    const bucket = this.buckets[this.buckets.length - 1]!;
    bucket.session = Math.min(this.maxSessionPerBucket, bucket.session + 1);
  }

  pushTransfer(mbps: number, now = Date.now()): void {
    if (mbps <= 0) {
      return;
    }
    this.ensureBucket(now);
    const bucket = this.buckets[this.buckets.length - 1]!;
    bucket.transferMbps = Math.max(bucket.transferMbps, mbps);
  }

  clear(): void {
    this.buckets = [];
    this.bucketStartMs = 0;
  }

  clearTransferTrack(): void {
    for (const bucket of this.buckets) {
      bucket.transferMbps = 0;
    }
  }

  getBuckets(): readonly ActivityBucket[] {
    return this.buckets;
  }

  transferScaleMax(floorMbps = 4): number {
    let peak = floorMbps;
    for (const bucket of this.buckets) {
      if (bucket.transferMbps > peak) {
        peak = bucket.transferMbps;
      }
    }
    return Math.max(floorMbps, peak * 1.15);
  }
}

export function bytesToActivityMbps(bytes: number, windowSec = 0.08): number {
  return Math.max(0.12, (bytes / (1024 * 1024)) / windowSec);
}

export function pushCompletedTransferSpike(
  history: ActivityHistory,
  resultMbps: number,
  prevResultMbps: number,
): number {
  if (resultMbps > 0 && prevResultMbps <= 0) {
    history.pushTransfer(resultMbps);
  }
  return resultMbps;
}
