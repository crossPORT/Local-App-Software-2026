# File transfer protocol — agent guide

Best practices for **ROCKETBX** payload moves over the SLS fabric USB link. Read this before changing `usb_transfer_core.cpp`, `TransferController`, payload staging, or progress/UI wiring.

**Related docs:** byte-level spec in [docs/PROTOCOL.md](../docs/PROTOCOL.md). Handshake coordination in [session.md](./session.md).

---

## Two layers on one wire

Both **session messages** and **file payloads** use the same USB bulk transfer engine:

1. 32-byte **ROCKETBX** header (magic + uint64 size + reserved)
2. Chunked bulk data (4 MiB frames on send path)

They differ in **size**, **timeout**, and **when** they run relative to the session handshake. Never start a payload transfer until **`ready`** has been exchanged (unless you are a low-level loopback test tool).

---

## ROCKETBX header (32 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | ASCII `"ROCKETBX"` |
| 8 | 8 | File size, uint64 LE |
| 16 | 16 | Reserved (zero) |

Constants: `core/include/usb_protocol.h`. Do not change magic/endpoints without FPGA team review.

---

## Send path rules (`send_file_core` / `send_on_port`)

- **Async bulk OUT** with **32 transfers in flight** (`kQueueDepth`).
- **Always submit full 4 MiB USB transfers** (`kChunkSize`); pad the last chunk with zeros.
- Receiver trims using `file_size` from header — padding is not part of the file.
- Progress callback ~once per completed chunk (worker thread — marshal to UI on main thread).
- Default per-chunk timeout: `kFileTimeoutMs` (8000 ms); override via config `transfer_timeout_ms` → `set_payload_timeout_ms()`.

**Do not** revert to synchronous bulk for large files — throughput and large-file correctness regress.

---

## Receive path rules (`receive_file_core` / `receive_on_port`)

- Blocking read for full 32-byte header (timeout per call — session vs payload).
- Async bulk IN in 4 MiB chunks; **ring buffer** with dedicated writer thread.
- A slot cannot be reused until the writer marks it FREE (prevents large-file corruption).
- On failure: **remove partial output file** (`receive_payload` / `cleanup_partial_receive_file`).

Session receives use `session_header_timeout_ms` (idle poll) or `handshake_poll_timeout_ms` (handshake). Payload receives after ready use `payload_header_timeout_ms`.

---

## USB topology and port index

| Fact | Implication |
|------|-------------|
| VID `0x1772`, PID `0x0006` | RocketBox hardware |
| Endpoints: OUT `0x02`, IN `0x81` | Single interface `0` |
| `port_index` = libusb enumeration order | **Not** the physical silkscreen label |
| Two cables on one PC | Two RocketBox App instances; `--port 0` / `--port 1`, or pick cable in **Connect USB** dialog |
| One cable | One app; `port_index` is usually `0` |

Permission errors → `scripts/setup-usb-access.sh`, replug cable.

---

## Mutex and sequencing (critical)

Each port has a **per-device USB mutex**. IN and OUT cannot run concurrently on the same cable.

### Outbound (sender)

1. Handshake completes (`ready` received).
2. **Pause session listener** so listen loop does not start another IN poll.
3. `send_on_port` with payload file path.
4. On complete: `finish_transfer`, resume listener, clear tight poll.

PWA: set `payloadSendPending` / pause listener when ready arrives — a stray IN poll blocks OUT for seconds on serialized WebUSB.

### Inbound (receiver)

1. After accept, send **`ready` first**.
2. Then call `receive_on_port` for payload.

Sending ready and receiving in the wrong order **deadlocks** on the same mutex.

---

## Staging and paths

- Outbound files are staged (single file or tar) before offer; temp files removed after send.
- Inbound path: `receive_folder` + sanitized `payload_name` from offer.
- Session temp files: `/tmp/slsfabric-session-*` (send) and `/tmp/slsfabric-sess-recv-*` (receive) — always deleted after use.
- Validate path traversal / meta safety — covered by unit tests; extend when adding fields.

---

## Timeouts summary

| Use | Constant | Typical value |
|-----|----------|---------------|
| Payload chunk stream | `kFileTimeoutMs` / config | 8000 ms per chunk |
| Session IN idle poll | `session_header_timeout_ms` | 2000 ms |
| Handshake IN poll | `handshake_poll_timeout_ms` | 350 ms |
| Payload header after ready | `payload_header_timeout_ms` | 15000 ms |
| Full file header (GUI handoff legacy note) | up to 120 s in vendor doc | Bounded in app by above |

A “hang” after ready usually means payload header never arrived — check cable, sender pause/listener race, or wrong port.

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

`booth_display_mib_s` and jitter affect **shown** speed in wx/PWA charts during transfer. They do **not** change USB chunk timing, handshake gaps, or throughput measurement on the wire.

Clear `fabricActivityMbps` / transfer chart state when idle — stale booth display rate looks like a phantom transfer.

---

## Threading and GUI

- All `*_core()` calls run on a **worker thread**.
- Progress and state updates → main/UI thread (`CallAfter` in wx, React state in PWA).
- **One transfer at a time** per orchestrator until complete or fail.
- **No cancel** in vendor contract yet — do not assume mid-flight abort without explicit design.
- **Do not start the server/app for the user** — they restart after builds.

Errors: surface `TransferResult.error_message` verbatim; core does not throw.

---

## Testing expectations

| Suite | Command | Covers |
|-------|---------|--------|
| Unit | `ctest --test-dir build -L unit` | Protocol math, session parse, partial cleanup, in-flight clamp |
| Integration | `ctest --test-dir build -L integration` | Full announce → offer → accept → ready → payload over `fabric_sim` |
| Hardware | `ctest --test-dir build -L hardware` | Real cables; auto-skips if < 2 devices |
| PWA | `cd apps/web && npm test` | Golden session fixtures parity |

Run integration tests after **any** change to `transfer_orchestrator.cpp`, `session_listener.cpp`, or payload receive cleanup.

Manual smoke:

```bash
./build/tools/usb-probe
./build/tools/usb-loopback-test /tmp/test.bin   # needs 2 cables
```

---

## Checklist before merging payload changes

- [ ] Payload only starts after session `ready`
- [ ] Listener paused before outbound payload send
- [ ] Inbound: ready sent before `receive_on_port`
- [ ] Header magic/size unchanged or coordinated with FPGA
- [ ] Async 4 MiB padded send path preserved
- [ ] Ring-buffer receive slot lifecycle preserved
- [ ] Failed receive deletes partial file
- [ ] Progress marshaled to UI thread
- [ ] Integration + relevant unit tests pass
- [ ] Booth/chart state cleared when transfer ends

---

## Anti-patterns (caused real bugs)

| Mistake | Symptom |
|---------|---------|
| Payload send while listener IN poll active | Blocked OUT; multi-second stall or timeout |
| Receive before sending ready | Self-deadlock on USB mutex |
| Treat session message as payload without handshake | Wrong file written / parse errors |
| Sync bulk for large files | Slow, corrupt, or incomplete transfers |
| Wrong `port_index` assumption | “No device” or transfer to wrong peer |
| Two processes claiming one cable | libusb claim errors, random failures |
| Booth rate left on UI after transfer | Green sparkline while idle |
| Edit core USB logic from GUI layer | Layering violation; hard to test |
