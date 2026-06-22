/** Time-bucketed USB activity: session message pulses + transfer throughput. */

export interface ActivityBucket {
  session: number;
  transferMbps: number;
}

export class ActivityHistory {
  private buckets: ActivityBucket[] = [];
  private bucketStartMs = 0;

  constructor(
    private readonly maxBuckets = 28,
    private readonly bucketMs = 400,
    private readonly maxSessionPerBucket = 5,
  ) {}

  private ensureBucket(now: number): void {
    if (this.buckets.length === 0) {
      this.bucketStartMs = now;
      this.buckets.push({ session: 0, transferMbps: 0 });
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
    const bucket = this.buckets[this.buckets.length - 1];
    bucket.session = Math.min(this.maxSessionPerBucket, bucket.session + 1);
  }

  pushTransfer(mbps: number, now = Date.now()): void {
    if (mbps <= 0) {
      return;
    }
    this.ensureBucket(now);
    const bucket = this.buckets[this.buckets.length - 1];
    bucket.transferMbps = Math.max(bucket.transferMbps, mbps);
  }

  clear(): void {
    this.buckets = [];
    this.bucketStartMs = 0;
  }

  /** Drop transfer bars when idle — session pulses stay visible. */
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
