import { describe, expect, it } from 'vitest';
import { SpeedHistory, bytesToActivityMbps } from './speed_history';

describe('bytesToActivityMbps', () => {
  it('converts recent byte delta to Mbps with floor', () => {
    const oneMiB = 1024 * 1024;
    expect(bytesToActivityMbps(oneMiB, 1)).toBeCloseTo(1, 5);
    expect(bytesToActivityMbps(0, 1)).toBe(0.12);
  });
});

describe('SpeedHistory', () => {
  it('deduplicates rapid near-identical samples', () => {
    const history = new SpeedHistory(10, 100);
    expect(history.push(10, 1000)).toBe(true);
    expect(history.push(10.01, 1050)).toBe(false);
    expect(history.getSamples()).toHaveLength(1);
  });

  it('caps sample count and computes scale max with headroom', () => {
    const history = new SpeedHistory(3, 0);
    history.push(10, 0);
    history.push(20, 1);
    history.push(30, 2);
    history.push(40, 3);
    expect(history.getSamples()).toEqual([20, 30, 40]);
    expect(history.scaleMax(8)).toBeCloseTo(40 * 1.12, 5);
  });

  it('clears samples', () => {
    const history = new SpeedHistory();
    history.push(5, 0);
    history.clear();
    expect(history.getSamples()).toEqual([]);
    expect(history.scaleMax()).toBeCloseTo(8 * 1.12, 5);
  });
});
