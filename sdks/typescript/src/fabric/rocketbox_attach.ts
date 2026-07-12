import { RocketBox } from '../rocketbox';
import type { Session } from '../session_core';
import type { Transport } from '../transport';
import { SimTransport } from '../transports/sim';
import { UsbTransport } from '../transports/usb';
import {
  clearSavedUsbPairing,
  countFabricDevices,
  findPairedDevice,
  hasSavedSerial,
  isFabricDevice,
  rememberSerial,
} from '../transports/usb_pairing';
import { fabricDebugLog } from './debug_log';
import { FabricUsbError } from './errors';
import {
  displayPortFromLeg,
  legFromDisplayPort,
  resolveFabricLegFromDevice,
} from './port';

export type AttachSessionOptions = {
  simulate: boolean;
  port: number;
  device?: USBDevice;
};

function wrapAttachError(displayPort: number, err: unknown): Error {
  const msg = err instanceof Error ? err.message : String(err);
  // boothLog expects fabric leg 0–3 (it +1 for the label).
  fabricDebugLog(legFromDisplayPort(displayPort), 'attach_fail', msg);
  if (msg === 'timeout' || /timeout/i.test(msg)) {
    return new FabricUsbError(
      'USB opened, but the local port did not reply on EP3 after ATTACH (host↔port control). Four endpoints are not enough — port control firmware must answer.',
    );
  }
  return err instanceof Error ? err : new Error(msg);
}

/** Attach RocketBox Session over UsbTransport (HW) or SimTransport. */
export async function attachSession(
  options: AttachSessionOptions,
): Promise<{ session: Session; transport: Transport }> {
  const { simulate, device } = options;
  let port = options.port;

  if (!simulate && device) {
    try {
      port = displayPortFromLeg(resolveFabricLegFromDevice(device));
    } catch {
      /* keep caller port if serial missing */
    }
  }

  const transport: Transport = simulate
    ? new SimTransport(port)
    : new UsbTransport({ device });
  const logLeg = legFromDisplayPort(port);
  fabricDebugLog(logLeg, 'attach_begin', simulate ? 'sim' : 'usb');
  let session: Session;
  try {
    session = await RocketBox.attach(transport, port);
  } catch (err) {
    try {
      await transport.disconnect();
    } catch {
      /* best effort */
    }
    throw wrapAttachError(port, err);
  }
  fabricDebugLog(logLeg, 'attach_ok', session.systemId);
  if (!simulate && transport instanceof UsbTransport) {
    const usbDevice = transport.getDevice();
    if (usbDevice) {
      rememberSerial(usbDevice);
    }
  }
  return { session, transport };
}

export async function countSimFabricDevices(): Promise<number> {
  return 4;
}

export {
  clearSavedUsbPairing,
  countFabricDevices,
  findPairedDevice,
  hasSavedSerial,
  isFabricDevice,
  rememberSerial,
};
