# File transfer protocol — agent guide

Best practices for **ROCKETBX** payload moves over the SLS fabric USB link. Read this before changing `usb_transfer_core.cpp`, `TransferController`, `fabric_link.ts`, payload staging, or progress/UI wiring.

**Related docs:** byte-level spec in [docs/PROTOCOL.md](../docs/PROTOCOL.md). Handshake coordination in [session.md](./session.md).

---

## Two layers on one wire

Both **session messages** and **file payloads** share the same 32-byte **ROCKETBX** header and bulk engine, but are tagged by **frame kind** (byte 16). The receiver uses frame kind to route traffic — session listener vs payload receive — instead of guessing from size alone.

Never start a payload transfer until **`ready`** has been exchanged (unless you are a low-level loopback test tool).

---

## ROCKETBX header (32 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | ASCII `"ROCKETBX"` |
| 8 | 8 | Body size, uint64 LE (file bytes or session message bytes) |
| 16 | 1 | **Frame kind** — required on every frame (no legacy untagged traffic) |
| 17 | 15 | Optional filename (NUL-padded ASCII; payload frames only) |

**Frame kinds** (`core/include/usb_protocol.h`, `apps/web/src/lib/fabric_frame.ts`):

| Value | Name | Use |
|-------|------|-----|
| `1` | `session` | Small FABRIC-SESSION-v1 text bodies (~100–200 B) |
| `2` | `payload` | File transfer after handshake |

Constants: `kFrameKindSession`, `kFrameKindPayload`, `kFrameKindOffset`, `kFrameFilenameOffset`, `kFrameFilenameMax`.

Session sends pass `kFrameKindSession` to `send_file_core`. Payload sends use `kFrameKindPayload` (default). Receives pass `expected_frame_kind` to `receive_file_core`:

- **`kFrameKindSession`** — session OUT send path
- **`kFrameKindPayload`** — inbound file after `ready`
- **`0`** — accept any kind (native session listener poll)

---

## Stray frames during payload wait

While waiting for a **payload** header, a **session** frame can still arrive (retransmit, announce, etc.). Both stacks skip or drain it instead of failing:

| Platform | Behavior |
|----------|----------|
| **C++** (`receive_file_core`) | If `expected_frame_kind == payload` and header is `session`, discard body bytes and keep polling until deadline |
| **PWA** (`FabricLink.receiveFileTransfer`) | Same: skip session frames (up to 16), optionally emit to session handler, then continue |

Do **not** treat a stray session frame as the payload header. Do **not** remove this skip logic — it fixed real “Unexpected frame kind in header” failures after successful transfers.

Invalid frame kind bytes (not 1 or 2) are a hard error on parse/send.

---

## Send path rules (`send_file_core` / `send_on_port`)

- **Async bulk OUT** with **32 transfers in flight** (`kQueueDepth`).
- **Always submit full 4 MiB USB transfers** (`kChunkSize`); pad the last chunk with zeros.
- Receiver trims using `file_size` from header — padding is not part of the file.
- Progress callback ~once per completed chunk (worker thread — marshal to UI on main thread).
- Default per-chunk timeout: `kFileTimeoutMs` (**8000 ms**); override via config `transfer_timeout_ms` → `set_payload_timeout_ms()`.
- Session message sends use the same engine with `kFrameKindSession` and a short send timeout (~2500 ms on PWA; `kSessionSendTimeoutMs` in native listener).

**Do not** revert to synchronous bulk for large files — throughput and large-file correctness regress.

---

## Receive path rules (`receive_file_core` / `receive_on_port`)

- Blocking read for full 32-byte header (timeout per call — session vs payload).
- Async bulk IN in 4 MiB chunks; **ring buffer** with dedicated writer thread.
- A slot cannot be reused until the writer marks it FREE (prevents large-file corruption).
- On failure: **remove partial output file** (`receive_payload` / `cleanup_partial_receive_file`).

| Receive purpose | Header timeout | Expected frame kind |
|-----------------|----------------|---------------------|
| Session listener (idle) | `session_header_timeout_ms` (2000 ms) | `0` (native) / session-only parse (PWA) |
| Handshake wait (tight poll) | `handshake_poll_timeout_ms` (350 ms) | same |
| Payload after `ready` | `payload_header_timeout_ms` (15000 ms) | `payload` |

---

## USB topology: port index vs fabric leg

Two different numbers appear in code and UI — do not conflate them.

| Concept | Meaning | Source |
|---------|---------|--------|
| **`port_index`** | libusb device enumeration order on this PC (0, 1, …) | `list_fabric_devices`, CLI `--port N`, Connect USB picker |
| **`fabric_leg`** | Which of the four fabric host legs this cable is wired to (internal 0–3, UI **Port 1–4**) | USB serial: `(parseInt(serial, 16) - 1) % 4` |

| Fact | Implication |
|------|-------------|
| VID `0x1772`, PID `0x0006` | RocketBox hardware |
| Endpoints: OUT `0x02`, IN `0x81` | Single interface `0` |
| **Four fabric legs** (`kFabricLegCount = 4`) | Up to four simultaneous booth stations on one fabric |
| Two cables on one PC | Two RocketBox App instances; different `port_index`, possibly different `fabric_leg` |
| One cable | One app; `port_index` is usually `0` |
| Cable must have USB serial | Without serial, fabric leg cannot be resolved (PWA throws; native falls back to `port_index`) |

Roster routing and announce `port=` use **fabric leg**, not libusb index. See [session.md](./session.md).

Permission errors → `scripts/setup-usb-access.sh`, replug cable.

---

## Mutex and sequencing (critical)

Each port has a **per-device USB mutex**. IN and OUT cannot run concurrently on the same cable (half-duplex fabric).

### Outbound (sender)

1. Handshake completes (`ready` received).
2. **Pause session listener** / set PWA `linkActivity` to `sending` (listen off).
3. `send_on_port` with payload file path, `kFrameKindPayload`.
4. On complete: `finish_transfer`, resume listener, return to idle.

PWA: `FabricLink` suspends listen, waits for active IN poll to retire, then runs OUT on the USB queue.

### Inbound (receiver)

1. After accept, send **`ready` first** (session frame).
2. Then call `receive_on_port` / `receiveFileTransfer` expecting **`payload`**.

Sending ready and receiving in the wrong order **deadlocks** on the same mutex.

---

## PWA: FabricLink USB queue

WebUSB has no native cancel/timeouts on bulk IN. `FabricLink` (`apps/web/src/lib/fabric_link.ts`) centralizes half-duplex access:

| Mechanism | Purpose |
|-----------|---------|
| **`usbTail` chain** | Serializes all USB work |
| **Tier 0 control queue** | Listen polls, session send/reply (priority 0 = listen, 1 = session send) |
| **Tier 1 payload** | `runUsb(1, …)` file chunks — runs after control jobs |
| **Listen modes** | `always` (idle), `handshake` (350 ms polls), `off` (payload send/receive) |
| **Pending header read** | One outstanding IN read reused across soft timeouts — fabric does not buffer |
| **Outbound prep** | Suspend listen → wait for IN poll → clear halts → send |

Native parity: `SessionListener` pause/resume + `ListenerPauseGuard` around session OUT sends.

---

## Staging and paths

- Outbound files are staged (single file or tar) before offer; temp files removed after send.
- Inbound path: `receive_folder` + sanitized `payload_name` from offer.
- Session temp files: written to temp paths for send; listener receives to `/tmp/slsfabric-sess-recv-*` — always deleted after use.
- Validate path traversal / meta safety — covered by unit tests; extend when adding fields.

---

## Timeouts summary

| Use | Constant | Typical value |
|-----|----------|---------------|
| Payload chunk stream | `kFileTimeoutMs` / config | **8000 ms** per chunk |
| Session message send | `kSessionFileTimeoutMs` / PWA `SESSION_SEND_TIMEOUT_MS` | 8000 / 2500 ms |
| Session IN idle poll | `session_header_timeout_ms` | 2000 ms |
| Handshake IN poll | `handshake_poll_timeout_ms` | 350 ms |
| Payload header after ready | `payload_header_timeout_ms` | 15000 ms |

A hang after ready usually means payload header never arrived — check cable, listener still polling, or wrong port/leg.

---

## Loopback vs peer transfer

| Mode | Tool / API | Notes |
|------|------------|-------|
| Same-process two-port | `loopback_transfer_core` | Mode-8 cross-connect; variable chunk sizes on link |
| Two apps / two cables | Normal send + receive | Full session handshake required |
| `usb-loopback-test` | CLI integrity check | No session layer |

Do not use loopback semantics in peer send/receive orchestration.

---

## Booth display rate (UI only)

`booth_display_mib_s` and jitter affect **shown** speed in wx/PWA charts during transfer. They do **not** change USB chunk timing, handshake gaps, or wire throughput.

Session rate stats (median / max / avg) come from **completed transfer `result_mbps`**, not booth display rate. Do not filter booth rates out of stats — that hid real metrics. Clear chart transfer track when live rate drops to zero after a transfer.

---

## Connect / disconnect (apps)

Both wx and PWA expose manual USB lifecycle (not auto-only):

| Action | wx | PWA |
|--------|-----|-----|
| Connect | **Connect USB** → device picker → start orchestrator | **Connect USB** → WebUSB permission + claim |
| Disconnect | **Disconnect** → stop orchestrator (blocked while busy) | **Disconnect** → release device |

One app instance per physical cable — WebUSB and libusb cannot share one device.

---

## Threading and GUI

- All `*_core()` calls run on a **worker thread**.
- Progress and state updates → main/UI thread (`CallAfter` in wx, React state in PWA).
- **One transfer at a time** per orchestrator until complete or fail.
- **Do not start the server/app for the user** — they restart after builds.

Errors: surface `TransferResult.error_message` verbatim; core does not throw.

---

## Testing expectations

| Suite | Command | Covers |
|-------|---------|--------|
| Unit | `ctest --test-dir build -L unit` | Frame kind, fabric leg, protocol math, session parse, partial cleanup |
| Integration | `ctest --test-dir build -L integration` | Full announce → offer → accept → ready → payload over `fabric_sim` |
| Hardware | `ctest --test-dir build -L hardware` | Real cables; auto-skips if < 2 devices |
| PWA | `cd apps/web && npm test` | `fabric_frame`, `fabric_protocol`, `fabric_link`, golden session fixtures |

Run integration tests after **any** change to `usb_transfer_core.cpp`, `transfer_orchestrator.cpp`, or `fabric_link.ts`.

---

## Checklist before merging payload changes

- [ ] Payload only starts after session `ready`
- [ ] Payload sends use `kFrameKindPayload`; session sends use `kFrameKindSession`
- [ ] Listener paused / PWA listen off before outbound payload send
- [ ] Inbound: ready sent before payload receive with `expected_frame_kind = payload`
- [ ] Stray session frames during payload wait are skipped, not fatal
- [ ] Header layout unchanged or coordinated with FPGA
- [ ] Async 4 MiB padded send path preserved
- [ ] Ring-buffer receive slot lifecycle preserved
- [ ] Failed receive deletes partial file
- [ ] Progress marshaled to UI thread
- [ ] Integration + relevant unit tests pass

---

## Anti-patterns (caused real bugs)

| Mistake | Symptom |
|---------|---------|
| Payload send while listener IN poll active | Blocked OUT; multi-second stall or timeout |
| Receive before sending ready | Self-deadlock on USB mutex |
| Treat session frame as payload without skip logic | “Unexpected frame kind in header”; corrupt receive |
| Missing or wrong frame kind on send | Peer rejects frame; parse errors |
| Sync bulk for large files | Slow, corrupt, or incomplete transfers |
| Confuse `port_index` with `fabric_leg` | Wrong peer routing, echo accepts, missing roster entries |
| Two processes claiming one cable | libusb / WebUSB claim errors |
| Booth rate used for session stats or chart scale | Phantom GiB/s, empty sparkline after transfer |
| Edit core USB logic from GUI layer | Layering violation; hard to test |
