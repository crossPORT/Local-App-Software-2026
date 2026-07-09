export interface Transport {
  readonly connected: boolean;
  init(): Promise<void>;
  disconnect(): Promise<void>;

  // Data Plane (EP1 / EP2)
  writeEP1(data: Uint8Array): void;
  onEP2Received(callback: (data: Uint8Array) => void): void;

  // Control Plane (EP3 / EP4)
  writeEP4(data: Uint8Array): Promise<[DataView, Uint8Array]>;
  onEP3Received(callback: (header: DataView, payload: Uint8Array) => void): void;
}
