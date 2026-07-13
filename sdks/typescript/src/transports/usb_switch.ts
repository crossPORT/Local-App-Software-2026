/** Port switch — raw 16-byte EP4 write (C++ `switch_port_core`). No EP3 reply. */

export const SWITCH_PACKET_SIZE = 16;

/** Control OUT endpoint address (matches C++ `kEndpointCtrlOut`). */
export const ENDPOINT_CTRL_OUT = 0x04;

/** Build switch packet; destDisplayPort 1–4 links, 0 clears. */
export function buildSwitchPacket(destDisplayPort: number): Uint8Array {
  const pkt = new Uint8Array(SWITCH_PACKET_SIZE);
  pkt[0] = destDisplayPort & 0x0f;
  return pkt;
}

export async function writeSwitch(
  device: USBDevice,
  ep4Out: number,
  destDisplayPort: number,
): Promise<void> {
  if (typeof device.clearHalt === 'function') {
    try {
      await device.clearHalt('out', ep4Out);
    } catch {
      /* best effort */
    }
  }
  const pkt = buildSwitchPacket(destDisplayPort);
  const result = await device.transferOut(ep4Out, pkt as BufferSource);
  if (result.status !== 'ok' || (result.bytesWritten ?? 0) !== SWITCH_PACKET_SIZE) {
    throw new Error(
      `Switch EP4 write failed status=${result.status} written=${result.bytesWritten ?? 0}`,
    );
  }
}
