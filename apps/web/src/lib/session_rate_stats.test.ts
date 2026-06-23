import { describe, expect, it } from 'vitest';
import { EMPTY_RATE_STATS, computeRateStats } from './session_rate_stats';

describe('computeRateStats', () => {
  it('returns empty stats when there are no positive rates', () => {
    expect(computeRateStats([])).toEqual(EMPTY_RATE_STATS);
    expect(computeRateStats([0, -1])).toEqual(EMPTY_RATE_STATS);
  });

  it('computes median for an odd number of rates', () => {
    const stats = computeRateStats([30, 10, 20]);
    expect(stats.count).toBe(3);
    expect(stats.median).toBe(20);
    expect(stats.max).toBe(30);
    expect(stats.average).toBeCloseTo(20, 5);
  });

  it('averages the two middle values for an even count', () => {
    const stats = computeRateStats([10, 20, 30, 40]);
    expect(stats.count).toBe(4);
    expect(stats.median).toBe(25);
    expect(stats.max).toBe(40);
    expect(stats.average).toBeCloseTo(25, 5);
  });

  it('ignores zero and negative rates', () => {
    const stats = computeRateStats([0, 12, 0, 24]);
    expect(stats.count).toBe(2);
    expect(stats.median).toBe(18);
    expect(stats.max).toBe(24);
  });
});
