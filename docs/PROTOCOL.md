# USB protocol

Device: **SLS fabric FPGA** — USB vendor `0x1772`, product `0x0006`.

Constants live in `core/include/usb_protocol.h`. Implementation in `core/src/usb_transfer_core.cpp`.

For **how the wx app calls the core** (threading, progress, port index), see `core/include/usb_transfer.h` and [AGENTS.md](../AGENTS.md). This document covers **on-the-wire** format.

## Endpoints

| Name | Address | Direction | Use |
|------|---------|-----------|-----|
| Data IN | `0x81` | device → host | Receive file payload |
| Data OUT | `0x02` | host → device | Send file payload |

Interface: `0`. Claim interface and detach kernel driver if active (handled in core).

## File transfer header

32-byte header:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | Magic ASCII `"ROCKETBX"` |
| 8 | 8 | File size (uint64, little-endian) |
| 16 | 16 | Reserved (zero) |

## Send path (`send_file_core`)

- Async bulk OUT with **32 transfers in flight** (`kQueueDepth`)
- **Always submit full 4 MiB USB transfers** (`kChunkSize`); last chunk zero-padded
- Receiver trims padding using `file_size` from header
- Progress callback ~once per completed chunk

## Receive path (`receive_file_core`)

- Blocking wait for full 32-byte header (`kFileTimeoutMs` per bulk read)
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
| `kFileTimeoutMs` | 120000 | File header and payload bulk transfers |

## Validation commands

```bash
./build/tools/usb-probe              # endpoints and device count
./build/tools/usb-loopback-test FILE # two-port file loopback
```

## When editing this document

Update if you change `usb_protocol.h` or transfer framing in `usb_transfer_core.cpp`.
