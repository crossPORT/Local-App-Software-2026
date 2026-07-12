/**
 * Manual/CI smoke: PWA-shaped flow over the live sim daemon (SDK only).
 * Run: npx vitest run src/lib/pwa_sdk_smoke.test.ts
 */
import { describe, expect, it } from 'vitest';
import { RocketBox, SimTransport } from '@rocketbox/sdk';
import { buildHeader } from '@rocketbox/sdk';
import { parseSessionPayload, serializeSessionMessage } from '@rocketbox/sdk';

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

describe('PWA SDK smoke against sim daemon', () => {
  it('attach, listSystems, connect, session message, file bytes', async () => {
    const t1 = new SimTransport(1, 1773);
    const t2 = new SimTransport(2, 1773);
    const s1 = await RocketBox.attach(t1, 1);
    const s2 = await RocketBox.attach(t2, 2);
    expect(s1.systemId).toBe('sys-port-1');
    expect(s2.systemId).toBe('sys-port-2');

    const systems = await s1.listSystems();
    expect(systems.some((s) => s.id === 'sys-port-2')).toBe(true);

    let gotOffer = false;
    let gotFile: Uint8Array | null = null;
    s2.onIncomingCircuit((conn) => {
      conn.onMessageReceived((bytes) => {
        const msg = parseSessionPayload(bytes);
        if (msg?.kind === 'offer') gotOffer = true;
      });
      conn.onReceived((bytes) => {
        if (bytes.length >= 4) {
          const len = new DataView(bytes.buffer, bytes.byteOffset, 4).getUint32(0);
          if (len === bytes.length - 4) return; // session frame
        }
        gotFile = bytes;
      });
    });

    const c1 = await s1.connect('sys-port-2');
    await sleep(50);
    await c1.sendMessage(
      serializeSessionMessage({
        kind: 'offer',
        session_id: 'smoke1',
        from_name: 'A',
        team: '',
        to_name: 'B',
        note: 'to_port=1',
        payload_type: 'file',
        payload_name: 'hi.bin',
        file_count: 1,
        total_bytes: 4,
      }),
    );
    const payload = new Uint8Array([1, 2, 3, 4]);
    const header = new Uint8Array(buildHeader(payload.length, { frameKind: 'payload', filename: 'hi.bin' }));
    const packet = new Uint8Array(header.length + payload.length);
    packet.set(header, 0);
    packet.set(payload, header.length);
    await c1.send(packet);

    for (let i = 0; i < 50 && (!gotOffer || !gotFile); i += 1) await sleep(20);
    expect(gotOffer).toBe(true);
    expect(gotFile).not.toBeNull();
    expect(gotFile!.length).toBeGreaterThanOrEqual(32 + 4);

    await c1.close();
    await t1.disconnect();
    await t2.disconnect();
  }, 15_000);
});
