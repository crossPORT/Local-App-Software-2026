import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { FabricLink } from './fabric_link';
import { CHUNK_SIZE, EP_IN, EP_OUT, HEADER_SIZE, buildHeader } from './fabric_protocol';
import { serializeSessionMessage, type FabricSessionMessage } from './fabric_session';

function makeMockDevice() {
  const outChunks: Uint8Array[] = [];
  const inQueue: Uint8Array[] = [];
  const device = {
    opened: true,
    configuration: 1,
    selectConfiguration: vi.fn(async () => {}),
    transferOut: vi.fn(async (_ep: number, data: BufferSource) => {
      const bytes =
        data instanceof ArrayBuffer
          ? new Uint8Array(data)
          : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      outChunks.push(bytes);
      return { status: 'ok' as const, bytesWritten: bytes.length };
    }),
    transferIn: vi.fn(async (_ep: number, byteCount: number) => {
      const parts: Uint8Array[] = [];
      let need = byteCount;
      while (need > 0 && inQueue.length > 0) {
        const head = inQueue[0]!;
        const take = Math.min(need, head.length);
        parts.push(head.subarray(0, take));
        if (take === head.length) {
          inQueue.shift();
        } else {
          inQueue[0] = head.subarray(take);
        }
        need -= take;
      }
      const merged = new Uint8Array(byteCount - need);
      let offset = 0;
      for (const part of parts) {
        merged.set(part, offset);
        offset += part.length;
      }
      return {
        status: 'ok' as const,
        data: new DataView(merged.buffer, merged.byteOffset, merged.byteLength),
      };
    }),
    releaseInterface: vi.fn(async () => {}),
    claimInterface: vi.fn(async () => {}),
    clearHalt: vi.fn(async () => {}),
  };
  return { device: device as unknown as USBDevice, outChunks, inQueue };
}

describe('FabricLink traffic controller', () => {
  beforeEach(() => {
    vi.stubGlobal('window', {
      setTimeout: (fn: () => void, ms?: number) => setTimeout(fn, ms),
      clearTimeout: (id: ReturnType<typeof setTimeout>) => clearTimeout(id),
      setInterval: (fn: () => void, ms?: number) => setInterval(fn, ms),
      clearInterval: (id: ReturnType<typeof setInterval>) => clearInterval(id),
    });
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('sends session frames with frameKind session before payload bytes', async () => {
    const { device, outChunks } = makeMockDevice();
    const link = new FabricLink(() => device, 0);
    const message: FabricSessionMessage = {
      kind: 'announce',
      session_id: '',
      from_name: 'A',
      team: '',
      to_name: '',
      note: 'port=1;receive=open',
      payload_type: 'file',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    };
    await link.sendSessionMessage(message);
    expect(outChunks.length).toBeGreaterThanOrEqual(2);
    const header = outChunks[0]!;
    expect(header.byteLength).toBe(HEADER_SIZE);
    expect(header[16]).toBe(1);
    const payload = outChunks[1]!;
    expect(payload).toEqual(serializeSessionMessage(message));
  });

  it('clears endpoint halts when the interface is reclaimed before a send', async () => {
    const { device } = makeMockDevice();
    const link = new FabricLink(() => device, 0);
    await link.sendSessionMessage({
      kind: 'announce',
      session_id: '',
      from_name: 'A',
      team: '',
      to_name: '',
      note: 'port=1;receive=open',
      payload_type: 'file',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    });
    const clearHalt = vi.mocked(device.clearHalt);
    expect(clearHalt).toHaveBeenCalledWith('out', EP_OUT);
    expect(clearHalt).toHaveBeenCalledWith('in', EP_IN);
  });

  it('sends session header before payload when session is queued first', async () => {
    const { device, outChunks } = makeMockDevice();
    const link = new FabricLink(() => device, 1);
    const payload = new Uint8Array([9, 8, 7]);
    await link.sendSessionMessage({
      kind: 'announce',
      session_id: '',
      from_name: 'B',
      team: '',
      to_name: '',
      note: '',
      payload_type: 'file',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    });
    await link.sendPayload(payload, undefined, 'x.bin');
    expect(outChunks[0]![16]).toBe(1);
    const payloadHeaderIndex = outChunks.findIndex((chunk) => chunk[16] === 2);
    expect(payloadHeaderIndex).toBeGreaterThan(0);
    expect(outChunks[payloadHeaderIndex]!.byteLength).toBe(HEADER_SIZE);
  });

  it('dispatches session events from background listen', async () => {
    vi.useFakeTimers();
    const { device, inQueue } = makeMockDevice();
    const link = new FabricLink(() => device, 0);
    link.setHandshakePollTimeoutMs(50);
    const message: FabricSessionMessage = {
      kind: 'offer',
      session_id: 's1',
      from_name: 'Alice',
      team: '',
      to_name: 'Bob',
      note: '',
      payload_type: 'file',
      payload_name: 'a.bin',
      file_count: 1,
      total_bytes: 4,
    };
    const body = serializeSessionMessage(message);
    inQueue.push(new Uint8Array(buildHeader(body.length, { frameKind: 'session' })));
    inQueue.push(body);
    const receivedPromise = new Promise<FabricSessionMessage>((resolve) => {
      link.subscribe((event) => {
        if (event.type === 'session') {
          resolve(event.message);
        }
      });
      link.setListenMode('handshake');
    });
    await vi.advanceTimersByTimeAsync(60);
    const received = await receivedPromise;
    expect(received.session_id).toBe('s1');
    link.stopListenLoop();
    vi.useRealTimers();
  });

  it('restarts background listen after a session send', async () => {
    vi.useFakeTimers();
    const { device } = makeMockDevice();
    const link = new FabricLink(() => device, 0);
    link.setHandshakePollTimeoutMs(50);
    link.setListenMode('always');
    await vi.advanceTimersByTimeAsync(100);
    expect(device.transferIn).toHaveBeenCalled();

    const before = vi.mocked(device.transferIn).mock.calls.length;
    const sendPromise = link.sendSessionMessage({
      kind: 'announce',
      session_id: 's1',
      from_name: 'A',
      team: '',
      to_name: '',
      note: 'port=1;receive=open',
      payload_type: 'file',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    });
    await vi.advanceTimersByTimeAsync(5000);
    await sendPromise;
    await vi.advanceTimersByTimeAsync(400);
    expect(vi.mocked(device.transferIn).mock.calls.length).toBeGreaterThan(before);

    link.stopListenLoop();
    vi.useRealTimers();
  });

  it('runs session sends before backlogged listen polls', async () => {
    vi.useFakeTimers();
    const events: string[] = [];
    let releaseListen: (() => void) | undefined;
    const listenGate = new Promise<void>((resolve) => {
      releaseListen = resolve;
    });
    const device = {
      opened: true,
      configuration: 1,
      selectConfiguration: vi.fn(async () => {}),
      transferOut: vi.fn(async () => {
        events.push('transferOut');
        return { status: 'ok' as const, bytesWritten: HEADER_SIZE };
      }),
      transferIn: vi.fn(async () => {
        events.push('transferIn');
        await listenGate;
        return Promise.reject(new DOMException('A transfer error has occurred.', 'NetworkError'));
      }),
      releaseInterface: vi.fn(async () => {}),
      claimInterface: vi.fn(async () => {}),
    };
    const link = new FabricLink(() => device as unknown as USBDevice, 0);
    link.setListenMode('off');
    const message: FabricSessionMessage = {
      kind: 'announce',
      session_id: 's1',
      from_name: 'A',
      team: '',
      to_name: '',
      note: '',
      payload_type: 'none',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    };

    link.setListenMode('always');
    await Promise.resolve();
    expect(events).toEqual(['transferIn']);

    void link.sendSessionMessage(message);
    await Promise.resolve();
    expect(events).toEqual(['transferIn']);

    releaseListen?.();
    await vi.advanceTimersByTimeAsync(3700);
    expect(events).toContain('transferOut');
    link.stopListenLoop();
    vi.useRealTimers();
  });

  it('treats idle fabric transferIn errors as a silent listen miss', async () => {
    vi.useFakeTimers();
    const device = {
      opened: true,
      configuration: 1,
      selectConfiguration: vi.fn(async () => {}),
      transferIn: vi.fn(() =>
        Promise.reject(new DOMException('A transfer error has occurred.', 'NetworkError')),
      ),
      transferOut: vi.fn(),
      releaseInterface: vi.fn(async () => {}),
      claimInterface: vi.fn(async () => {}),
    };
    const link = new FabricLink(() => device as unknown as USBDevice, 1);
    link.setListenMode('off');
    link.setHandshakePollTimeoutMs(50);
    link.setListenMode('always');

    await vi.advanceTimersByTimeAsync(400);
    link.stopListenLoop();

    expect(device.transferIn).toHaveBeenCalled();
    expect(device.releaseInterface).not.toHaveBeenCalled();
    vi.useRealTimers();
  });

  it('keeps listening after a send whose interface recovery fails', async () => {
    vi.useFakeTimers();
    let claimShouldFail = false;
    const device = {
      opened: true,
      configuration: 1,
      selectConfiguration: vi.fn(async () => {}),
      // Idle fabric: every IN poll returns no data (status ok, empty buffer).
      transferIn: vi.fn(async () => ({
        status: 'ok' as const,
        data: new DataView(new ArrayBuffer(0)),
      })),
      transferOut: vi.fn(async () => ({ status: 'ok' as const, bytesWritten: 0 })),
      releaseInterface: vi.fn(async () => {}),
      claimInterface: vi.fn(async () => {
        // Mimic a reconnect-window claim failure that does not match any
        // "transient contention" branch, so ensureInterfaceReady throws.
        if (claimShouldFail) {
          throw new Error('The device was disconnected.');
        }
      }),
      clearHalt: vi.fn(async () => {}),
    };
    const link = new FabricLink(() => device as unknown as USBDevice, 0);
    link.setHandshakePollTimeoutMs(50);
    link.setListenMode('always');
    await vi.advanceTimersByTimeAsync(120);
    const before = vi.mocked(device.transferIn).mock.calls.length;
    expect(before).toBeGreaterThan(0);

    // The send's bus prep aborts the stuck transfer, which reclaims the
    // interface — and that claim fails. The send must reject AND must not leave
    // the listen loop permanently suspended.
    claimShouldFail = true;
    const sendOutcome = link
      .sendSessionMessage({
        kind: 'announce',
        session_id: '',
        from_name: 'A',
        team: '',
        to_name: '',
        note: 'port=1;receive=open',
        payload_type: 'file',
        payload_name: '',
        file_count: 0,
        total_bytes: 0,
      })
      .then(
        () => 'sent',
        () => 'failed',
      );
    await vi.advanceTimersByTimeAsync(3000);
    expect(await sendOutcome).toBe('failed');

    // Recovery is healthy again; inbound polling must resume.
    claimShouldFail = false;
    link.ensureListening();
    const afterFail = vi.mocked(device.transferIn).mock.calls.length;
    await vi.advanceTimersByTimeAsync(600);
    expect(vi.mocked(device.transferIn).mock.calls.length).toBeGreaterThan(afterFail);

    link.stopListenLoop();
    vi.useRealTimers();
  });

  it('receives a peer frame after idle listen polls have already timed out', async () => {
    // Models real WebUSB bulk-IN: transferIn parks until data arrives (it has
    // no internal timeout) and the hub delivers a buffered frame FIFO to the
    // OLDEST outstanding request. A listen poll that "times out" on the JS side
    // leaves its transferIn outstanding; if it is never cancelled it becomes an
    // orphan that steals the next frame, so the active poll never sees it.
    vi.useFakeTimers();
    const queue: number[] = [];
    type Waiter = {
      byteCount: number;
      resolve: (r: USBInTransferResult) => void;
      reject: (e: unknown) => void;
    };
    const waiters: Waiter[] = [];
    const service = (): void => {
      while (waiters.length > 0 && queue.length > 0) {
        const w = waiters.shift()!;
        const take = Math.min(w.byteCount, queue.length);
        const bytes = new Uint8Array(queue.splice(0, take));
        w.resolve({
          status: 'ok',
          data: new DataView(bytes.buffer),
        } as USBInTransferResult);
      }
    };
    const device = {
      opened: true,
      configuration: 1,
      selectConfiguration: vi.fn(async () => {}),
      transferOut: vi.fn(async () => ({ status: 'ok' as const, bytesWritten: 0 })),
      transferIn: vi.fn(
        (_ep: number, byteCount: number) =>
          new Promise<USBInTransferResult>((resolve, reject) => {
            waiters.push({ byteCount, resolve, reject });
            service();
          }),
      ),
      // Real WebUSB cancels pending transfers when the interface is released.
      releaseInterface: vi.fn(async () => {
        while (waiters.length > 0) {
          waiters.shift()!.reject(new DOMException('transfer cancelled', 'AbortError'));
        }
      }),
      claimInterface: vi.fn(async () => {}),
      clearHalt: vi.fn(async () => {}),
    };
    const pushFrame = (bytes: Uint8Array): void => {
      for (const b of bytes) {
        queue.push(b);
      }
      service();
    };

    const link = new FabricLink(() => device as unknown as USBDevice, 0);
    let received: FabricSessionMessage | null = null;
    link.subscribe((event) => {
      if (event.type === 'session') {
        received = event.message;
      }
    });

    link.setListenMode('always');
    // Let several idle listen polls time out first so any orphaned transferIn
    // accumulates before the peer frame arrives.
    await vi.advanceTimersByTimeAsync(1500);

    const message: FabricSessionMessage = {
      kind: 'announce',
      session_id: 'alice-1',
      from_name: 'Alice',
      team: '',
      to_name: '',
      note: 'port=2;receive=open',
      payload_type: 'file',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    };
    const body = serializeSessionMessage(message);
    const frame = new Uint8Array(HEADER_SIZE + body.length);
    frame.set(new Uint8Array(buildHeader(body.length, { frameKind: 'session' })), 0);
    frame.set(body, HEADER_SIZE);
    pushFrame(frame);

    await vi.advanceTimersByTimeAsync(2000);

    expect(received).not.toBeNull();
    expect(received!.from_name).toBe('Alice');

    link.stopListenLoop();
    vi.useRealTimers();
  });

  it('receives payload frames with frameKind payload', async () => {
    const { device, inQueue } = makeMockDevice();
    const link = new FabricLink(() => device, 0);
    const body = new Uint8Array(CHUNK_SIZE);
    body[0] = 42;
    inQueue.push(new Uint8Array(buildHeader(body.length, { frameKind: 'payload', filename: 'f.bin' })));
    inQueue.push(body);
    const result = await link.receiveFileTransfer(1000, body.length);
    expect(result.filename).toBe('f.bin');
    expect(result.data[0]).toBe(42);
  });

  it('keeps the IN endpoint armed so a frame transmitted between polls is not dropped', async () => {
    // Models the REAL no-buffer fabric: a peer frame is only delivered if a
    // transferIn (IN read) is OUTSTANDING at the instant it is transmitted —
    // there is no hub buffer to hold it. A listener that disarms the endpoint
    // between polls misses every frame that lands in the gap, so cross-leg
    // discovery takes minutes. The listener must keep one read armed ~always.
    vi.useFakeTimers();
    type Waiter = {
      byteCount: number;
      resolve: (r: USBInTransferResult) => void;
      reject: (e: unknown) => void;
    };
    let waiter: Waiter | null = null;
    // Continuation of an in-progress transmit (body bytes after the header).
    let bodyPending: Uint8Array | null = null;
    const device = {
      opened: true,
      configuration: 1,
      selectConfiguration: vi.fn(async () => {}),
      transferOut: vi.fn(async () => ({ status: 'ok' as const, bytesWritten: 0 })),
      transferIn: vi.fn(
        (_ep: number, byteCount: number) =>
          new Promise<USBInTransferResult>((resolve, reject) => {
            if (bodyPending) {
              const take = Math.min(byteCount, bodyPending.length);
              const out = bodyPending.slice(0, take);
              bodyPending = bodyPending.length > take ? bodyPending.slice(take) : null;
              resolve({ status: 'ok', data: new DataView(out.buffer) } as USBInTransferResult);
              return;
            }
            waiter = { byteCount, resolve, reject };
          }),
      ),
      releaseInterface: vi.fn(async () => {
        if (waiter) {
          waiter.reject(new DOMException('transfer cancelled', 'AbortError'));
          waiter = null;
        }
        bodyPending = null;
      }),
      claimInterface: vi.fn(async () => {}),
      clearHalt: vi.fn(async () => {}),
    };
    // Deliver a frame to the armed read, or DROP it if the endpoint is idle.
    const transmit = (frame: Uint8Array): boolean => {
      if (!waiter) {
        return false;
      }
      const w = waiter;
      waiter = null;
      bodyPending = frame.slice(HEADER_SIZE);
      const header = frame.slice(0, HEADER_SIZE);
      const take = Math.min(w.byteCount, header.length);
      w.resolve({ status: 'ok', data: new DataView(header.slice(0, take).buffer) } as USBInTransferResult);
      return true;
    };

    const link = new FabricLink(() => device as unknown as USBDevice, 0);
    let received: FabricSessionMessage | null = null;
    link.subscribe((event) => {
      if (event.type === 'session') {
        received = event.message;
      }
    });

    link.setListenMode('always');
    // Run the listener through several poll cycles, then transmit at a moment
    // that falls in a poll GAP for a disarm-on-timeout listener (~800ms).
    await vi.advanceTimersByTimeAsync(800);

    const message: FabricSessionMessage = {
      kind: 'announce',
      session_id: 'alice-1',
      from_name: 'Alice',
      team: '',
      to_name: '',
      note: 'port=2;receive=open',
      payload_type: 'file',
      payload_name: '',
      file_count: 0,
      total_bytes: 0,
    };
    const body = serializeSessionMessage(message);
    const frame = new Uint8Array(HEADER_SIZE + body.length);
    frame.set(new Uint8Array(buildHeader(body.length, { frameKind: 'session' })), 0);
    frame.set(body, HEADER_SIZE);
    const delivered = transmit(frame);

    await vi.advanceTimersByTimeAsync(1000);

    expect(delivered).toBe(true);
    expect(received).not.toBeNull();
    expect(received!.from_name).toBe('Alice');

    link.stopListenLoop();
    vi.useRealTimers();
  });

  it('streams a multi-chunk payload to completion without aborting on the session timeout', async () => {
    // The no-buffer half-duplex fabric retires a chunk's transferOut only once
    // the receiver pulls it — far slower than a small session frame. Model a
    // 3s-per-transfer chunk (longer than the 2.5s session send timeout). The
    // payload path must use a generous timeout and must NOT release/claim
    // mid-stream, or it aborts an otherwise-healthy multi-chunk transfer.
    vi.useFakeTimers();
    const outChunks: Uint8Array[] = [];
    const device = {
      opened: true,
      configuration: 1,
      selectConfiguration: vi.fn(async () => {}),
      transferOut: vi.fn(
        (_ep: number, data: BufferSource) =>
          new Promise<USBOutTransferResult>((resolve) => {
            const bytes =
              data instanceof ArrayBuffer
                ? new Uint8Array(data)
                : new Uint8Array(
                    (data as ArrayBufferView).buffer,
                    (data as ArrayBufferView).byteOffset,
                    (data as ArrayBufferView).byteLength,
                  );
            const copy = bytes.slice(0);
            window.setTimeout(() => {
              outChunks.push(copy);
              resolve({ status: 'ok', bytesWritten: copy.length } as USBOutTransferResult);
            }, 3000);
          }),
      ),
      transferIn: vi.fn(async () => ({
        status: 'ok' as const,
        data: new DataView(new ArrayBuffer(0)),
      })),
      releaseInterface: vi.fn(async () => {}),
      claimInterface: vi.fn(async () => {}),
      clearHalt: vi.fn(async () => {}),
    };
    const link = new FabricLink(() => device as unknown as USBDevice, 0);
    link.setListenMode('off');

    const payload = new Uint8Array(CHUNK_SIZE + 100);
    payload[0] = 11;
    payload[CHUNK_SIZE] = 22;
    const outcome = link.sendPayload(payload, undefined, 'big.bin').then(
      () => 'ok',
      (err) => `fail:${(err as Error).message}`,
    );

    // Prep + header(3s) + chunk0(3s) + chunk1(3s) — well under the payload timeout.
    await vi.advanceTimersByTimeAsync(20000);

    expect(await outcome).toBe('ok');
    const payloadHeader = outChunks.find((chunk) => chunk.length === HEADER_SIZE && chunk[16] === 2);
    expect(payloadHeader).toBeDefined();
    const dataChunks = outChunks.filter((chunk) => chunk.length === CHUNK_SIZE);
    expect(dataChunks.length).toBe(2);
    expect(dataChunks[0]![0]).toBe(11);
    expect(dataChunks[1]![0]).toBe(22);

    vi.useRealTimers();
  });
});
