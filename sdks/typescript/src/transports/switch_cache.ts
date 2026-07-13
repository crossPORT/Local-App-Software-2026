import { sleep } from './bulk_io';
import { writeSwitch } from './usb_switch';
import type { UsbEndpoints } from './usb_ids';

/** Post-switch settle before ROCKETBX data (ms). */
export const SWITCH_SETTLE_MS = 8;

/** Tracks last EP4 dest so switchIfNeeded can no-op. */
export class SwitchDestCache {
  private lastDest: number | null = null;
  /** Session/listen hold — scheduled announce must not rotate away. */
  private preserve_ = false;

  get last(): number | null {
    return this.lastDest;
  }

  get preserve(): boolean {
    return this.preserve_;
  }

  /** Mark current dest as sticky (offer / inbound listen). */
  markPreserve(): void {
    this.preserve_ = this.lastDest != null && this.lastDest >= 1 && this.lastDest <= 4;
  }

  clear(): void {
    this.lastDest = null;
    this.preserve_ = false;
  }

  /** Write EP4 only when dest changed; settle only after a real switch. */
  async switchIfNeeded(
    device: USBDevice,
    eps: UsbEndpoints,
    destDisplayPort: number,
  ): Promise<boolean> {
    if (this.lastDest === destDisplayPort) {
      return false;
    }
    await writeSwitch(device, eps.ep4Out, destDisplayPort);
    this.lastDest = destDisplayPort;
    if (destDisplayPort === 0) {
      this.preserve_ = false;
    }
    await sleep(SWITCH_SETTLE_MS);
    return true;
  }
}
