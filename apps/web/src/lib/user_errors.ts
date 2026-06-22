import { FabricUsbError } from './fabric_usb';

/** User closed the browser USB picker without choosing a device. */
export function isUsbPickerCancel(err: unknown): boolean {
  if (err instanceof DOMException) {
    return err.name === 'NotFoundError' || err.message.includes('No device selected');
  }
  const message = (err as Error)?.message ?? '';
  return message.includes('No device selected');
}

/** Map connect/reconnect failures to short UI copy; null = nothing to show (e.g. picker cancel). */
export function formatUsbConnectError(err: unknown): string | null {
  if (isUsbPickerCancel(err)) {
    return null;
  }
  if (err instanceof FabricUsbError) {
    return err.message;
  }
  const message = (err as Error)?.message ?? String(err);
  if (message.includes('WebUSB unavailable')) {
    return 'WebUSB is not available — use Chrome or Edge on localhost.';
  }
  if (message.includes('Access denied') || message.includes('SecurityError')) {
    return 'USB access denied — close other RocketBox tabs, click Forget USB device, then connect again.';
  }
  if (message.includes('interface is busy') || message.includes('Could not claim')) {
    return message;
  }
  if (message.includes('disconnected')) {
    return 'USB device disconnected — plug in the cable and try Connect USB again.';
  }
  if (message.includes('No saved cable') || message.includes('No USB device')) {
    return 'Cable not found — plug it in, then click Connect USB.';
  }
  return 'Could not connect — check the cable and try Connect USB again.';
}

export function formatTransferError(err: unknown): string {
  if (err instanceof FabricUsbError) {
    return err.message;
  }
  const message = (err as Error)?.message ?? String(err);
  if (message.includes('disconnected')) {
    return 'USB disconnected during transfer.';
  }
  return message || 'Transfer failed — try again.';
}
