# USB protocol

Device: **RocketBox** hardware — USB vendor `0x1772`, product `0x0006`.

Constants live in `core/include/usb_protocol.h`. Implementation in `core/src/usb_transfer_core.cpp`. PWA mirror: `apps/web/src/lib/fabric_protocol.ts`, `fabric_frame.ts`.

For **how apps call the core** (threading, progress, port index vs fabric leg), see [AGENTS.md](../AGENTS.md), [protocols/file-transfer.md](../protocols/file-transfer.md), and [protocols/session.md](../protocols/session.md). This document covers **on-the-wire** format.

## Endpoints

| Name | Address | Direction | Use |
|------|---------|-----------|-----|
| Data IN | `0x81` | device → host | Receive frames |
| Data OUT | `0x02` | host → device | Send frames |

Interface: `0`. Claim interface and detach kernel driver if active (handled in core).

## Fabric topology

- **Four host legs** per fabric (`kFabricLegCount = 4`), labeled Port 1–4 in UI.
- Leg identity comes from cable USB serial: `(serial_hex - 1) % 4`.
- **`port_index`** in libusb is the device enumeration order on the host PC — not the fabric leg.

## File transfer header (32 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | Magic ASCII `"ROCKETBX"` |
| 8 | 8 | Body size (uint64, little-endian) — file bytes or session message bytes |
| 16 | 1 | **Frame kind** (required) |
| 17 | 15 | Filename (NUL-padded ASCII; optional, payload frames) |

### Frame kinds

| Byte | Constant | Traffic |
|------|----------|---------|
| `1` | `kFrameKindSession` | FABRIC-SESSION-v1 text bodies |
| `2` | `kFrameKindPayload` | File payload after handshake |

There is **no legacy untagged mode**. Invalid kind bytes are rejected.

## Send path (`send_file_core`)

- Async bulk OUT with **32 transfers in flight** (`kQueueDepth`)
- **Always submit full 4 MiB USB transfers** (`kChunkSize`); last chunk zero-padded
- Receiver trims padding using body size from header
- Progress callback ~once per completed chunk
- `frame_kind` argument selects session vs payload header (default: payload)

## Receive path (`receive_file_core`)

- Blocking wait for full 32-byte header (timeout argument per call)
- **`expected_frame_kind`:** `0` = accept any; `1` = session only; `2` = payload only
- When expecting payload, **session frames are skipped** (body discarded, poll continues)
- Async bulk IN of 4 MiB per transfer
- Ring buffer (`kPoolSize` = queue depth + 2) with dedicated writer thread to disk
- Slot cannot be reused until writer marks it FREE (prevents large-file corruption)

## Loopback (`loopback_transfer_core`)

Two-port file cross-connect (same PC, two cables):

1. Requires ≥ 2 fabric devices (port indices 0 and 1 typical)
2. Open send port and recv port
3. Send 32-byte header on send port; read header on recv port
4. Pipelined chunk transfer with double-buffered disk/USB overlap (`usb_transfer_chunk`)
5. Compare received temp file to source; delete temp file

Variable-size chunks on the cross-connect path (not 4 MiB padded frames).

## Timeouts

| Constant | Value | Use |
|----------|-------|-----|
| `kFileTimeoutMs` | 8000 | Payload bulk chunk stream (override via config) |
| `kSessionFileTimeoutMs` | 8000 | Session message send backstop |
| `kSessionHeaderTimeoutMs` | 2000 | Session listener idle poll |
| `kPayloadHeaderTimeoutMs` | 15000 | Payload header wait after `ready` |
| `kAcceptTimeoutSec` | 60 | Human accept window |
| `kReadyTimeoutSec` | 20 | Ready wait after accept |

Handshake poll timeout (350 ms) lives in `session_handshake.*`, not `usb_protocol.h`.

## Validation commands

```bash
./build/tools/usb-probe              # endpoints and device count
./build/tools/usb-loopback-test FILE # two-port file loopback
ctest --test-dir build -L unit       # frame kind, protocol unit tests
```

## When editing this document

Update if you change `usb_protocol.h`, frame kind semantics, or transfer framing in `usb_transfer_core.cpp`. Also update [protocols/file-transfer.md](../protocols/file-transfer.md) and [protocols/session.md](../protocols/session.md) if behavior guidance changes.
