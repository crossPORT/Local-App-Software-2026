import { FabricUsbError } from './errors';
import { INTERFACE_NUMBER, PRODUCT_ID, VENDOR_ID } from './protocol';
import type { FabricUsbSession } from './usb_session';
import {
  clearSavedSerial,
  findPairedDevice,
  getSavedSerial,
  hasSavedSerial,
  isFabricDevice,
  rememberSerial,
} from './usb_pairing';
import { mapUsbOpenError, openFabricDeviceWithRetry, resetDeviceHandle } from './usb_open';
import { webUsbBlockedReason } from './webusb_avail';

export async function usbConnect(session: FabricUsbSession): Promise<string> {
  const blocked = webUsbBlockedReason();
  if (blocked) {
    throw new FabricUsbError(blocked);
  }
  await session.disconnect();
  let device: USBDevice;
  try {
    device = await navigator.usb.requestDevice({
      filters: [{ vendorId: VENDOR_ID, productId: PRODUCT_ID }],
    });
  } catch (err) {
    if (err instanceof DOMException && (err.name === 'NotFoundError' || err.message.includes('No device selected'))) {
      throw err;
    }
    throw mapUsbOpenError(err);
  }
  await resetDeviceHandle(device);
  await openFabricDeviceWithRetry(device);
  session.device = device;
  rememberSerial(device);
  session.refreshResolvedLeg();
  return session.describeDevice();
}

export async function usbReconnectKnown(session: FabricUsbSession): Promise<string> {
  if (!navigator.usb) {
    throw new FabricUsbError('WebUSB unavailable');
  }
  if (!hasSavedSerial()) {
    throw new FabricUsbError('No saved cable for this window — click Connect USB');
  }
  await session.disconnect();
  const devices = (await navigator.usb.getDevices()).filter(isFabricDevice);
  const device = findPairedDevice(devices);
  if (!device) {
    clearSavedSerial();
    throw new FabricUsbError(
      'Previously paired device not found — tap Connect USB and pick your cable in the browser dialog',
    );
  }
  await resetDeviceHandle(device);
  try {
    await openFabricDeviceWithRetry(device);
  } catch (err) {
    throw err instanceof FabricUsbError
      ? err
      : new FabricUsbError('Saved device handle is stale — click Reconnect USB or power-cycle the RocketBox');
  }
  session.device = device;
  rememberSerial(device);
  session.refreshResolvedLeg();
  return session.describeDevice();
}

export async function usbDisconnect(session: FabricUsbSession): Promise<void> {
  session.link.setListenMode('off');
  session.link.stopListenLoop();
  if (!session.device?.opened) {
    session.device = null;
    return;
  }
  try {
    await session.device.releaseInterface(INTERFACE_NUMBER);
  } catch {
    /* best effort */
  }
  try {
    await session.device.close();
  } finally {
    session.device = null;
  }
}

export async function usbForgetThisDevice(session: FabricUsbSession): Promise<void> {
  const savedSerial = getSavedSerial();
  await session.disconnect();
  if (!navigator.usb) {
    return;
  }
  const devices = (await navigator.usb.getDevices()).filter(isFabricDevice);
  for (const device of devices) {
    if (savedSerial && device.serialNumber !== savedSerial) {
      continue;
    }
    try {
      if (device.opened) {
        try {
          await device.releaseInterface(INTERFACE_NUMBER);
        } catch {
          /* best effort */
        }
        await device.close();
      }
      await device.forget();
    } catch {
      /* best effort */
    }
  }
  clearSavedSerial();
}

export async function usbResetConnection(session: FabricUsbSession): Promise<string> {
  if (session.device?.serialNumber) {
    rememberSerial(session.device);
  } else if (!getSavedSerial()) {
    throw new FabricUsbError('USB not connected — click Connect USB');
  }
  await session.disconnect();
  return session.reconnectKnown();
}
