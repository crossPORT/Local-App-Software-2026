import { useEffect, useRef, useState } from 'react';
import { computeRateStats, type RateStats } from '../lib/session_rate_stats';

/** Accumulates completed-transfer rates for the current browser session. */
export function useSessionRates(resultMbps: number): { rates: number[]; stats: RateStats } {
  const [rates, setRates] = useState<number[]>([]);
  const prevRate = useRef(0);

  useEffect(() => {
    if (resultMbps > 0 && prevRate.current <= 0) {
      setRates((prev) => [...prev, resultMbps]);
    }
    prevRate.current = resultMbps;
  }, [resultMbps]);

  return { rates, stats: computeRateStats(rates) };
}
