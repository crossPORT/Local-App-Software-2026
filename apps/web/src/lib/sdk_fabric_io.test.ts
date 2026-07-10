import { describe, expect, it, vi } from 'vitest';
import { sendFileOnCircuit } from './sdk_fabric_io';
import { HEADER_SIZE } from './fabric_protocol';

describe('sendFileOnCircuit', () => {
  it('chunks payload and reports progress before completion', async () => {
    const sent: Uint8Array[] = [];
    const progress: Array<[number, number]> = [];
    const connection = {
      send: vi.fn(async (bytes: Uint8Array) => {
        sent.push(bytes.slice());
      }),
    };

    const payload = new Uint8Array(64);
    payload.fill(7);

    await sendFileOnCircuit(connection as never, payload, 'spike.bin', (done, total) => {
      progress.push([done, total]);
    });

    expect(sent.length).toBeGreaterThanOrEqual(2);
    expect(sent[0]!.length).toBe(HEADER_SIZE);
    expect(progress[0]).toEqual([0, 64]);
    expect(progress[progress.length - 1]).toEqual([64, 64]);
    expect(progress.some(([done]) => done > 0 && done < 64 || done === 64)).toBe(true);
  });
});
