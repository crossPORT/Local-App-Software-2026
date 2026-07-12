import type { PlaneState } from './rocketbox_plane';
import type { Transport } from '../transport';
import { UsbTransport } from '../transports/usb';
import { pickDevice } from '../transports/usb_open';
import { forgetUsbDevice, resolvePairedUsbDevice } from '../transports/usb_pair_ops';
import { FabricUsbError } from './errors';

export type TransportLifeCtx = {
  simulate: boolean;
  plane: PlaneState;
  transport: Transport | null;
  attachFresh: (device?: USBDevice) => Promise<void>;
  teardown: () => Promise<void>;
  describeDevice: () => string;
};

export function usbDeviceOf(transport: Transport | null): USBDevice | null {
  return transport instanceof UsbTransport ? transport.getDevice() : null;
}

export async function doConnect(ctx: TransportLifeCtx): Promise<string> {
  // Pick the device BEFORE any teardown awaits so the click gesture stays valid.
  let device: USBDevice | undefined;
  if (!ctx.simulate) {
    device = await pickDevice();
  }
  await ctx.attachFresh(device);
  return ctx.describeDevice();
}

export async function doReconnectKnown(ctx: TransportLifeCtx): Promise<string> {
  if (ctx.simulate) {
    await ctx.attachFresh();
    return ctx.describeDevice();
  }
  try {
    const device = await resolvePairedUsbDevice();
    await ctx.attachFresh(device);
    return ctx.describeDevice();
  } catch (err) {
    throw err instanceof FabricUsbError
      ? err
      : new FabricUsbError((err as Error).message || 'Reconnect failed');
  }
}

export async function doForget(ctx: TransportLifeCtx): Promise<void> {
  const device = usbDeviceOf(ctx.transport);
  await ctx.teardown();
  if (ctx.simulate) {
    return;
  }
  await forgetUsbDevice(device);
}

export async function doReset(ctx: TransportLifeCtx): Promise<string> {
  try {
    await ctx.plane.connection?.close();
  } catch {
    /* best effort */
  }
  ctx.plane.connection = null;
  ctx.plane.dataBuf.clear();
  if (!ctx.simulate && ctx.transport?.recover) {
    await ctx.transport.recover();
  }
  return ctx.describeDevice();
}
