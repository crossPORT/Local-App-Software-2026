export class FabricUsbError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'FabricUsbError';
  }
}
