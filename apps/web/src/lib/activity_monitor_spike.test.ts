import { describe, expect, it } from 'vitest';
import {
  ACTIVITY_BUCKET_MS,
  ACTIVITY_MAX_BUCKETS,
  ActivityHistory,
  pushCompletedTransferSpike,
} from './activity_history';

describe('ActivityHistory smooth scroll', () => {
  it('primes a full-width empty strip', () => {
    const history = new ActivityHistory();
    history.prime(1_000);
    expect(history.getBuckets()).toHaveLength(ACTIVITY_MAX_BUCKETS);
  });

  it('exposes a fractional phase between bucket boundaries', () => {
    const history = new ActivityHistory();
    history.prime(1_000);
    expect(history.scrollPhase(1_000)).toBeCloseTo(0, 3);
    expect(history.scrollPhase(1_000 + ACTIVITY_BUCKET_MS / 2)).toBeCloseTo(0.5, 2);
  });

  it('keeps transfer samples while time advances', () => {
    const history = new ActivityHistory();
    history.prime(1_000);
    history.pushTransfer(50, 1_000);
    history.scrollPhase(1_000 + ACTIVITY_BUCKET_MS * 3);
    expect(history.getBuckets().some((b) => b.transferMbps === 50)).toBe(true);
  });
});

describe('pushCompletedTransferSpike', () => {
  it('records a teal transfer bar on the rising edge of resultMbps', () => {
    const history = new ActivityHistory();
    pushCompletedTransferSpike(history, 7168, 0);
    expect(history.getBuckets().at(-1)?.transferMbps).toBe(7168);
  });
});
