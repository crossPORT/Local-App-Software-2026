/** Aggregate stats for completed-transfer rates within a session (MiB/s). */

export interface RateStats {
  count: number;
  median: number;
  max: number;
  average: number;
}

export const EMPTY_RATE_STATS: RateStats = { count: 0, median: 0, max: 0, average: 0 };

/** Compute count, median, max and average from a list of per-transfer rates. */
export function computeRateStats(rates: readonly number[]): RateStats {
  const values = rates.filter((rate) => rate > 0);
  if (values.length === 0) {
    return EMPTY_RATE_STATS;
  }
  const sorted = [...values].sort((a, b) => a - b);
  const count = sorted.length;
  const mid = Math.floor(count / 2);
  const median = count % 2 === 0 ? (sorted[mid - 1] + sorted[mid]) / 2 : sorted[mid];
  const max = sorted[count - 1];
  const average = sorted.reduce((sum, value) => sum + value, 0) / count;
  return { count, median, max, average };
}
