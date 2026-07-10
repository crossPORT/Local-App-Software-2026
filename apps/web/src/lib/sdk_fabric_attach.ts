import { RocketBox, SimTransport, UsbTransport, type Session, type Transport } from '@rocketbox/sdk';
import { fabricSimEnabled } from '../../sim/fabric_sim';

const SELECTED_SERIAL_KEY = 'rocketbox_usb_serial';

export function portFromUrl(): number {
  const params = new URLSearchParams(window.location.search);
  const p = Number.parseInt(params.get('port') ?? '1', 10);
  return Number.isFinite(p) && p >= 1 && p <= 4 ? p : 1;
}

export async function pickUsbDevice(): Promise<USBDevice> {
  const usb = navigator.usb;
  if (!usb) throw new Error('WebUSB is not available');
  const paired = (await usb.getDevices()).filter(
    (d) => d.vendorId === 0x1772 && d.productId === 0x0006,
  );
  const saved = sessionStorage.getItem(SELECTED_SERIAL_KEY);
  const match = saved ? paired.find((d) => d.serialNumber === saved) : null;
  const device =
    match ??
    (paired.length === 1
      ? paired[0]!
      : await usb.requestDevice({
          filters: [{ vendorId: 0x1772, productId: 0x0006 }],
        }));
  if (device.serialNumber) {
    sessionStorage.setItem(SELECTED_SERIAL_KEY, device.serialNumber);
  }
  return device;
}

export async function attachSession(
  port: number,
): Promise<{ session: Session; transport: Transport; device: USBDevice | null }> {
  let device: USBDevice | null = null;
  let transport: Transport;
  if (fabricSimEnabled()) {
    transport = new SimTransport(port);
  } else {
    device = await pickUsbDevice();
    transport = new UsbTransport({ device });
  }
  const session = await RocketBox.attach(transport, port);
  return { session, transport, device };
}

export function clearSavedSerial(): void {
  sessionStorage.removeItem(SELECTED_SERIAL_KEY);
}

export function hasSavedSerial(): boolean {
  return Boolean(sessionStorage.getItem(SELECTED_SERIAL_KEY));
}

export async function countFabricDevices(): Promise<number> {
  if (fabricSimEnabled()) return 4;
  const usb = navigator.usb;
  if (!usb) return 0;
  const devices = await usb.getDevices();
  return devices.filter((d) => d.vendorId === 0x1772 && d.productId === 0x0006).length;
}
