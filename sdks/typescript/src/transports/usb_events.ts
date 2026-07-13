/** Subscribe to WebUSB disconnect via SDK (PWA must not touch navigator.usb). */
export function subscribeUsbDisconnect(
  handler: (device: USBDevice) => void,
): () => void {
  const usb = typeof navigator !== 'undefined' ? navigator.usb : undefined;
  if (!usb) {
    return () => undefined;
  }
  const listener = (event: USBConnectionEvent) => {
    handler(event.device);
  };
  usb.addEventListener('disconnect', listener);
  return () => usb.removeEventListener('disconnect', listener);
}
