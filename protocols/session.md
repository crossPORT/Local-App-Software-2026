# Session protocol — agent guide

Best practices for **FABRIC-SESSION-v1** coordination over the SLS fabric USB link. Read this before changing `session_listener`, `transfer_orchestrator`, `web_transfer_orchestrator`, `fabric_link`, or handshake timing.

**Related docs:** wire-level USB framing in [docs/PROTOCOL.md](../docs/PROTOCOL.md). Low-level send/receive rules in [file-transfer.md](./file-transfer.md).

---

## Core constraint: the fabric does not buffer

The FPGA USB path has **no session queue**. A message exists on the wire only while the sender is transmitting and the receiver has an active **USB IN poll** armed.

If the receiver is sleeping, paused, blocked on OUT, or between polls with a long idle gap, **the message is lost**. Timeouts that follow are often timing bugs, not “bad cables” or “wrong peer.”

Design every handshake step assuming messages can be dropped unless both sides actively manage poll timing.

---

## What a session message is

- Text format: header `FABRIC-SESSION-v1`, then `key=value` lines.
- Kinds: `announce`, `offer`, `accept`, `decline`, `ready`.
- On the wire: serialized to a temp file, sent as a **ROCKETBX frame with `frame_kind = session`** (same bulk engine as payloads, ~100–200 B body).
- Golden fixtures: `tests/fixtures/session/` — **always update and run tests** when changing parse/serialize.

Required fields for correlation: `session_id`, `from`, `team`. Offers add `to`, `payload_name`, `total_bytes`, etc.

Implementation files: `fabric_session_message.*` (C++), `fabric_session.ts` (PWA).

---

## Four-leg fabric routing

The RocketBox fabric exposes **four host-facing legs** (internal index **0–3**, UI label **Port 1–4**).

| Concept | Rule |
|---------|------|
| **Local leg identity** | `(USB_serial_hex - 1) % 4` — see `fabric_port.cpp` / `fabric_port.ts` |
| **Announce wire port** | `port=1` … `port=4` in announce `note` (legacy `0–3` still parsed) |
| **Remote peer leg** | Parsed from announce; must differ from local leg (`resolveRemoteFabricLeg`) |
| **Echo rejection** | Ignore announces where `instance_id` matches local, or (legacy) same name + same leg |
| **Default remote guess** | `(my_leg + 1) % 4` when parsing ambiguous notes |

**`port_index`** (libusb enumeration on this PC) is **not** the fabric leg. Routing, roster keys, and UI “Port N” labels use **fabric leg** from cable serial.

Up to four booth stations can share one fabric; stagger announces on PWA startup by `leg * (interval / 4)` to reduce collision when all four connect at once.

---

## End-to-end handshake flow

```
Sender (Bob)                          Receiver (Alice)
────────────────                      ─────────────────
announce (periodic)  ───────────────►  roster learns peer + leg
offer [session]      ───────────────►  prompt / auto-accept
                     ◄───────────────  accept [session]
                     ◄───────────────  ready [session]
ROCKETBX payload [2] ───────────────►  receive_on_port → disk
```

Frame kind `[1]` = session, `[2]` = payload — see [file-transfer.md](./file-transfer.md).

### Phase rules

| Phase | Sender must | Receiver must |
|-------|-------------|---------------|
| **Offer** | Pause listener → send offer (session frame) → **grace gap** → resume listener in **tight poll** | Listener active; read offer |
| **Accept** | Tight IN poll; optional **one offer retransmit** ~950 ms after first send | Wait **`accept_reply_delay_ms`** (≥ 2× grace) before sending accept |
| **Ready** | Wait for ready (accept may have been dropped — **ready implies accept**) | Send **ready before** starting payload receive (USB mutex) |
| **Payload** | Pause listener → `send_on_port` (payload frame) | Already sent ready → `receive_on_port` (expect payload) |

---

## Timing constants (defaults)

Defined in `core/include/usb_protocol.h` and mirrored in `session_handshake.*` / `session_handshake.ts`.

| Constant | Default | Purpose |
|----------|---------|---------|
| `accept_ready_gap_ms` | 400 | Sender/receiver gap after offer or accept so partner can re-arm IN |
| `accept_reply_delay_ms` | 800 (2× gap) | Receiver waits before accept so sender listener is polling |
| `accept_timeout_sec` | 60 | Human accept window (sender countdown) |
| `accept_receiver_margin_sec` | 3 | Receiver dialog expires slightly before sender timeout |
| `ready_timeout_sec` | 20 | After accept, wait for ready + payload start |
| `session_header_timeout_ms` | 2000 | Normal idle IN poll timeout |
| `handshake_poll_timeout_ms` | 350 | Short IN poll while sender waits for accept/ready |
| `payload_header_timeout_ms` | 15000 | Receiver waits for payload header after ready |

Announce interval: **15 s** native (`kAnnounceIntervalMs`), **10 s** PWA (`ANNOUNCE_INTERVAL_MS`). Peer stale ~45 s.

**Rule:** `accept_reply_delay_ms` **must exceed** `accept_ready_gap_ms`. Lowering gaps speeds transfers but increases drop risk on real hardware.

---

## Listener and link activity

### Native (`SessionListener`)

1. **Poll IN before OUT** — never hold IN blocked while expecting a reply.
2. **`ListenerPauseGuard` / `pause()`** before any session OUT send.
3. **Tight poll** on sender while waiting for accept/ready:
   - 350 ms header timeout (`set_tight_poll(true)`)
   - **No** long idle sleep between failed polls during tight poll
4. **Offer grace:** after sending offer, wait `accept_ready_gap_ms` before expecting accept.
5. **One offer retransmit** if accept not seen by `accept_reply_delay_ms + accept_ready_gap_ms + 150`.
6. Session receives use `expected_frame_kind = 0` (any); body must parse as FABRIC-SESSION-v1.

### PWA (`WebTransferOrchestrator` + `FabricLink`)

**Link activity** FSM drives listen mode:

| `linkActivity` | `FabricLink` listen mode | Announces |
|----------------|--------------------------|-----------|
| `idle` | `always` (200 ms soft polls) | Allowed |
| `handshake` | `handshake` (350 ms polls) | Suppressed |
| `receiving` / `sending` | `off` | Suppressed |

`setLinkActivity` is called at each handshake/transfer boundary. Do not leave listen on during payload send — WebUSB OUT blocks until peer IN drains.

**FabricLink** also serializes USB via Tier-0 control queue (listen + session) before Tier-1 payload chunks. See [file-transfer.md](./file-transfer.md).

---

## Announces

- Peers come from live **`announce`**, not static config lists alone (config seeds names only).
- Announce `note` fields: `port=N;receive=open|ask_first|busy;instance=<uuid>` (see `announce_note.*`).
- Offers with `to_name` must match local `display_name` or be rejected.
- **Suppress announces** while outbound offer pending, inbound dialog open, busy, or non-idle link activity — avoids USB bus contention during handshake even though frame kinds now differ.
- PWA initial announce delayed by `fabric_leg * (interval / 4)` + jitter.

---

## Idempotency and lost messages

- **`handle_ready`:** if accept was dropped, treat ready as implicit accept (`awaiting_ready = true`). Do not fail solely because accept never arrived.
- **Retransmit offer once** on sender; duplicates with same `session_id` refresh dialog expiry (PWA) or are ignored while accepting (native `offer_dup_*` logs).
- **After successful inbound receive:** ignore duplicate offers/accepts for that `session_id` (`last_completed_inbound_session_id_` in C++; guard while `accepting_inbound_session_id_` set).
- **Decline / timeout:** clear outbound offer, staged temp files, tight poll, and link activity → idle.

---

## Roster and identity

- Roster entries store peer **fabric leg** from announce, not libusb index.
- Receive policy from announce: `open` (auto-accept), `ask_first` (prompt), `busy` (treated as open for offers in current code).
- **`instance_id`** in announce distinguishes multiple apps on different legs with the same display name.
- **One app instance per USB cable** — WebUSB and libusb cannot share one device.

---

## UI vs protocol (common confusion)

| UI state | Protocol phase | Correct copy |
|----------|----------------|--------------|
| `waitingForPartner` + outbound offer, no accept yet | Handshake wait | “Waiting for \<peer\> to accept…” |
| `busy` + payload in flight | File transfer | “Sending / Receiving …” |
| Booth display rate | **Display only** | Does not change USB timing or session stats |
| Activity monitor med/max/avg | Completed session transfer rates | Needs ≥1 finished transfer; not booth rate |

Do not show “Transfer in progress” during accept wait.

Both wx and PWA: **Connect USB** / **Disconnect** under the USB device panel; disconnect blocked while transfer busy.

---

## Platform parity

Session logic exists in **two places** — keep them aligned:

| Concern | C++ | PWA |
|---------|-----|-----|
| Orchestrator | `lib/session/transfer_orchestrator.cpp` | `apps/web/src/lib/web_transfer_orchestrator.ts` |
| USB half-duplex | `session_listener.cpp` + pause guards | `fabric_link.ts` queue + listen modes |
| Handshake defaults | `session_handshake.cpp` | `session_handshake.ts` |
| Fabric leg / port label | `fabric_port.cpp` | `fabric_port.ts` |
| Announce note parse | `announce_note.cpp` | `announce_note.ts` |
| Frame kind on wire | `usb_protocol.h` + send/receive in core | `fabric_frame.ts` + `fabric_protocol.ts` |
| Event log | `booth_log.*` | `booth_log.ts` |

After session changes: `ctest --test-dir build -L integration` and `cd apps/web && npm test`.

---

## Checklist before merging session changes

- [ ] Session OUT frames use `kFrameKindSession`
- [ ] Offer send pauses listener and respects post-offer grace
- [ ] Sender uses tight poll + optional retransmit while waiting for accept
- [ ] Receiver delays accept by `accept_reply_delay_ms`
- [ ] Gap between accept and ready uses `accept_ready_gap_ms`
- [ ] Ready sent before inbound payload receive
- [ ] Ready handler tolerates missing accept
- [ ] Duplicate inbound session guarded after successful receive
- [ ] Announces suppressed during handshake / transfer
- [ ] Fabric leg from serial used for roster routing (not libusb index)
- [ ] Tight poll / link activity cleared in `finish_transfer` / cancel paths
- [ ] Golden session fixtures still pass (C++ + vitest)
- [ ] Integration handshake tests pass

---

## Anti-patterns (caused real bugs)

| Mistake | Symptom |
|---------|---------|
| Long sleep between IN polls during handshake | Intermittent accept timeout |
| Send accept immediately after reading offer | Accept lost; sender times out |
| Send announce during outbound handshake | Bus contention; delayed accept/ready |
| Skip listener pause before session OUT | Race with listen loop; dropped reads |
| Assume timeout = user didn’t click Accept | Often a dropped message on no-buffer link |
| Use libusb `port_index` as fabric leg | Peers missing/wrong on wx vs PWA |
| Static peer list instead of announce | “Peer not found” despite cable connected |
| Two apps on one cable | Device claim failures, flaky behavior |
| Re-process accept after inbound payload complete | Frame kind errors; duplicate receive |
| Filter booth display rate from session stats | med/max/avg hidden after transfer |
