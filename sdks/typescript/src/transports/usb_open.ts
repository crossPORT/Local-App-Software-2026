import {
  DEFAULT_ENDPOINTS,
  INTERFACE_NUMBER,
  PRODUCT_ID,
  VENDOR_ID,
  type UsbEndpoints,
} from './usb_ids';

export function discoverEndpoints(device: USBDevice): UsbEndpoints {
  const cfg = device.configuration as {
    interfaces?: Array<{
      alternate: { endpoints: Array<{ endpointNumber: number; direction: string; type: string }> };
    }>;
  } | null;
  const eps = cfg?.interfaces?.[INTERFACE_NUMBER]?.alternate?.endpoints ?? [];
  const outs = eps
    .filter((e) => e.direction === 'out' && e.type === 'bulk')
    .map((e) => e.endpointNumber)
    .sort((a, b) => a - b);
  const inns = eps
    .filter((e) => e.direction === 'in' && e.type === 'bulk')
    .map((e) => e.endpointNumber)
    .sort((a, b) => a - b);

  // C++ usb_protocol: data OUT 0x02, data IN 0x81 (endpoint numbers 2 and 1).
  if (outs.length >= 2 && inns.length >= 2) {
    return {
      ep1Out: outs.includes(2) ? 2 : outs.includes(1) ? 1 : outs[0]!,
      ep4Out: outs.includes(4) ? 4 : outs.find((n) => n !== 2 && n !== 1) ?? outs[1]!,
      ep2In: inns.includes(1) ? 1 : inns.includes(2) ? 2 : inns[0]!,
      ep3In: inns.includes(3) ? 3 : inns.find((n) => n !== 1 && n !== 2) ?? inns[1]!,
    };
  }
  if (outs.length === 1 && inns.length === 1) {
    throw new Error(
      'Device exposes only 2 bulk endpoints (legacy). Need 4-EP port firmware (EP3/EP4 host control + EP1/EP2 fabric data).',
    );
  }
  return { ...DEFAULT_ENDPOINTS };
}

async function clearHalts(device: USBDevice, eps: UsbEndpoints): Promise<void> {
  if (typeof device.clearHalt !== 'function') return;
  for (const [dir, num] of [
    ['out', eps.ep1Out],
    ['out', eps.ep4Out],
    ['in', eps.ep2In],
    ['in', eps.ep3In],
  ] as const) {
    try {
      await device.clearHalt(dir, num);
    } catch {
      /* best effort */
    }
  }
}

/** Drop a stale open/claim left by HMR or a prior tab on this origin. */
export async function releaseDevice(device: USBDevice | null): Promise<void> {
  if (!device?.opened) return;
  try {
    await device.releaseInterface(INTERFACE_NUMBER);
  } catch {
    /* best effort */
  }
  try {
    await device.close();
  } catch {
    /* best effort */
  }
}

export async function openAndClaim(device: USBDevice): Promise<UsbEndpoints> {
  await releaseDevice(device);
  await device.open();
  if (device.configuration == null) await device.selectConfiguration(1);
  try {
    await device.claimInterface(INTERFACE_NUMBER);
  } catch {
    await device.releaseInterface(INTERFACE_NUMBER).catch(() => undefined);
    await device.claimInterface(INTERFACE_NUMBER);
  }
  const eps = discoverEndpoints(device);
  await clearHalts(device, eps);
  return eps;
}

/** USB reset so firmware drops a ghost attach from a dead JS session. */
export async function resetAndClaim(device: USBDevice): Promise<UsbEndpoints> {
  await releaseDevice(device);
  try {
    await device.open();
    await device.reset();
  } catch {
    /* best effort — some hosts reject reset; openAndClaim still helps */
  }
  await releaseDevice(device);
  // Let the port re-enumerate after bus reset before we claim again.
  await new Promise((r) => setTimeout(r, 150));
  return openAndClaim(device);
}

export async function pickDevice(existing?: USBDevice): Promise<USBDevice> {
  if (existing) return existing;
  const usb = navigator.usb;
  if (!usb) throw new Error('WebUSB is not available in this environment');

  return usb.requestDevice({
    filters: [
      { vendorId: VENDOR_ID, productId: PRODUCT_ID },
      { vendorId: VENDOR_ID },
    ],
  });
}
