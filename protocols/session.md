# Session protocol — agent guide

Best practices for **FABRIC-SESSION-v1** coordination over the SLS fabric USB link. Read this before changing `session_listener`, `transfer_orchestrator`, `web_transfer_orchestrator`, or handshake timing.

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
- Implementation: serialized to a temp file, then sent via the **same ROCKETBX file transfer** as payloads (small ~100–200 B “files”).
- Inbox path convention: `.fabric-session/<session_id>.msg` (see `fabric_session_message.*`).
- Golden fixtures: `tests/fixtures/session/` — **always update and run tests** when changing parse/serialize.

Required fields for correlation: `session_id`, `from`, `team`. Offers add `to`, `payload_name`, `total_bytes`, etc.

---

## End-to-end handshake flow

```
Sender (Bob)                          Receiver (Alice)
────────────────                      ─────────────────
announce (periodic)  ───────────────►  roster learns peer
offer                ───────────────►  prompt / auto-accept
                     ◄───────────────  accept
                     ◄───────────────  ready
ROCKETBX payload     ───────────────►  receive_on_port → disk
```

### Phase rules

| Phase | Sender must | Receiver must |
|-------|-------------|---------------|
| **Offer** | Pause listener → send offer → **grace gap** → resume listener in **tight poll** | Listener active; read offer |
| **Accept** | Tight IN poll; optional **one offer retransmit** ~950 ms after first send | Wait **`accept_reply_delay_ms`** (≥ 2× grace) before sending accept |
| **Ready** | Wait for ready (accept may have been dropped — **ready implies accept**) | Send **ready before** starting payload receive (USB mutex — see file-transfer guide) |
| **Payload** | Pause listener → `send_on_port` | Already sent ready → `receive_on_port` |

---

## Timing constants (defaults)

Defined in `core/include/usb_protocol.h` and mirrored in `session_handshake.*`.

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

**Rule:** `accept_reply_delay_ms` **must exceed** `accept_ready_gap_ms`. Lowering gaps speeds transfers but increases drop risk on real hardware.

---

## Listener rules (do not break these)

1. **Poll IN before OUT** — never hold IN blocked while expecting a reply.
2. **`ListenerPauseGuard` / `pauseListener()`** before any session OUT send.
3. **Tight poll** on sender while `outboundOffer` is active:
   - 350 ms header timeout
   - **No** 50–100 ms idle sleep between failed polls
4. **Offer grace:** after sending offer, do not immediately IN-poll; give receiver time to read (PWA: `offerReceiveGraceUntil`).
5. **Resume listener** after grace before waiting for accept.
6. **One offer retransmit** if accept not seen by `accept_reply_delay_ms + accept_ready_gap_ms + 150`.
7. **Disable announce** while outbound offer is pending — announces use ROCKETBX framing; receiver may mis-parse them as payload.
8. **Per-port USB mutex:** accept and ready cannot overlap with payload on the same device; receiver sends ready first, then receives.

---

## Idempotency and lost messages

- **`handle_ready`:** if accept was dropped, treat ready as implicit accept (`awaiting_ready_ = true`). Do not fail the transfer solely because accept never arrived.
- **Retransmit offer once** on sender; do not spam — duplicates are handled by `session_id` matching.
- **Decline / timeout:** clear `outbound_offer`, staged temp files, and **`set_tight_poll(false)`**.

---

## Roster and identity

- Peers come from live **`announce`**, not static config lists.
- Announce `note` carries port hint and receive policy (`open`, `ask_first`, etc.).
- Offers with `to_name` must match local `display_name` or be rejected.
- **`?port=1` in PWA** is an identity hint; local USB index may still be `0` with one cable.
- **One app instance per USB cable** — WebUSB and libusb cannot share one device.

---

## UI vs protocol (common confusion)

| UI state | Protocol phase | Correct copy |
|----------|----------------|--------------|
| `waitingForPartner` + outbound offer, no accept yet | Handshake wait | “Waiting for \<peer\> to accept…” |
| `busy` + payload in flight | File transfer | “Sending / Receiving …” |
| Demo display rate | **Display only** | Does not change USB timing |

Do not show “Transfer in progress” during accept wait.

---

## Platform parity

Session logic exists in **two places** — keep them aligned:

| Concern | C++ | PWA |
|---------|-----|-----|
| Orchestrator | `apps/demo/transfer_orchestrator.cpp` | `apps/web/src/lib/web_transfer_orchestrator.ts` |
| Handshake defaults | `session_handshake.cpp` | `session_handshake.ts` |
| Listener | `session_listener.cpp` | listener loop in web orchestrator |

After session changes: `ctest --test-dir build -L integration` and `cd apps/web && npm test`.

---

## Checklist before merging session changes

- [ ] Offer send pauses listener and respects post-offer grace
- [ ] Sender uses tight poll + optional retransmit while waiting for accept
- [ ] Receiver delays accept by `accept_reply_delay_ms`
- [ ] Gap between accept and ready uses `accept_ready_gap_ms`
- [ ] Ready sent before inbound payload receive
- [ ] Ready handler tolerates missing accept
- [ ] Announces suppressed during outbound handshake
- [ ] Tight poll cleared in `finish_transfer` / cancel paths
- [ ] Golden session fixtures still pass (C++ + vitest)
- [ ] Integration handshake tests pass

---

## Anti-patterns (caused real bugs)

| Mistake | Symptom |
|---------|---------|
| Long sleep between IN polls during handshake | Intermittent accept timeout |
| Send accept immediately after reading offer | Accept lost; sender times out |
| Send announce during outbound offer wait | Receiver parses garbage / wrong phase |
| Skip listener pause before session OUT | Race with listen loop; dropped or duplicated reads |
| Assume timeout = user didn’t click Accept | Often a dropped message on no-buffer link |
| Static peer list instead of announce | “Peer not found” despite cable connected |
| Two apps on one cable | Device claim failures, flaky behavior |
