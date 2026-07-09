# RocketBox PWA WebUSB Buffer Tuning Guide

This document outlines the performance, memory, and stability trade-offs associated with the **USB Read Buffer Size** (or "shot glass" size) settings in the WebUSB Progressive Web App (PWA).

This is a critical reference for both **humans** (developers tuning performance) and **AI agents** (who need to understand the architecture).

---

## 1. Context: Native Hardware vs. WebUSB Sandboxing

The native RocketBox C++ SDK talks directly to the OS kernel via `libusb` using a high-performance, hardcoded chunk size:
* **C++ Core Chunk Size:** `4 MiB` (`kChunkSize` in `usb_protocol.h`).
* **C++ Behavior:** The desktop app allocates large contiguous physical blocks for DMA transfers and monitors OS-level memory limits (`usbfs_memory_mb`) to avoid kernel exhaustion.

In the **WebUSB PWA**, we cannot safely use 4 MiB buffers because the web browser operates inside a highly sandboxed environment with strict garbage collection constraints.

---

## 2. The Browser's Two Bottlenecks

### A. IPC & Promise Resolution Overhead
Every time WebUSB requests data from a physical endpoint (`device.transferIn()`), the browser has to cross multiple application boundaries:
$$\text{PWA (V8 JS Engine)} \xrightarrow{\text{IPC}} \text{Chrome Renderer} \xrightarrow{\text{IPC}} \text{Chrome Browser Process} \xrightarrow{\text{System Call}} \text{OS Kernel}$$

In JavaScript, each transfer requires resolving an asynchronous `Promise`. If the chunk size is too small, Chrome spends more CPU time scheduling Promises and serializing data across IPC boundaries than actually moving physical bytes.

### B. V8 Garbage Collection (GC) & Frame Stutters
JavaScript is garbage-collected. If the PWA allocates and discards massive multi-megabyte buffers (like 4 MB) dozens of times per second:
1. It triggers severe **V8 Heap Fragmentation**.
2. It forces V8's "Scavenger" garbage collector to perform heavy, blocking collection sweeps.
3. This leads to **UI Jitter / Jank**—the interface freezes for 50–100ms, stuttering progress bars and locking user input.

---

## 3. Buffer Size Options & Trade-Offs

To maintain perfect 60 FPS UI responsiveness while maximizing throughput, the PWA exposes four production-tuned buffer size configurations:

| Buffer Option | Raw Speed | CPU / IPC Overhead | V8 GC Pressure | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`16kb`** *(Legacy)* | Slow | 🔴 Extremely High | 🟢 None (Ultra-stable) | Legacy or low-power hardware (e.g. old Chromebooks) |
| **`64kb`** *(Standard)* | Moderate | 🟡 High | 🟢 Negligible | Baseline fallback |
| **`256kb`** *(Optimal)* | **Fast** | 🟢 Low | 🟢 Very Low | **Default Production Choice** (Best balance of speed and 60fps smoothness) |
| **`1mb`** *(Performance)* | **Maximum** | 🟢 Minimal | 🟡 Moderate (Minor GC) | Fast hardware / SuperSpeed USB 3.0 transfers |

---

## 4. Codebase Reference (For Agents & Humans)

If you are modifying or debugging the transfer behavior, here are the key integration points:

* **PWA Schema Definition:** `apps/web/src/lib/types.ts`
  Exposes the `usb_read_buffer_size` parameter as part of the `IdentityProfile` interface.
  
* **Configuration Defaults:** `apps/web/src/lib/config.ts`
  Initializes `defaultIdentityProfile` with `usb_read_buffer_size: '256kb'`.

* **Settings Dialog UI:** `apps/web/src/components/SettingsDialog.tsx`
  Renders the dropdown select component letting users save their preference to `localStorage`.

* **SDK Implementation (Sim):** `apps/web/sim/fabric_sim_session.ts`
  Method `getChunkSizeStrategy()` dynamically retrieves this setting from local storage and translates it into physical slicing constraints in `sendBytes` to faithfully mimic hardware buffer behavior.
