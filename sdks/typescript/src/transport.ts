/**
 * Local RocketBox port transport.
 *
 * Control (EP4/EP3): host↔port only — fabric never sees these.
 * Data (EP1/EP2): payload across the crossport fabric once a circuit is live.
 */
export interface Transport {
  readonly connected: boolean;
  init(): Promise<void>;
  disconnect(): Promise<void>;
  /** Fired on unexpected link loss (not intentional disconnect()). */
  onDisconnected(callback: () => void): void;
  /** Optional: USB reset + reclaim after a ghost firmware attach (HMR / crash). */
  recover?(): Promise<void>;

  /** Data plane → crossport fabric (OUT). */
  writeEP1(data: Uint8Array): void;
  /** Data plane ← crossport fabric (IN). */
  onEP2Received(callback: (data: Uint8Array) => void): void;

  /**
   * Control plane → port (OUT). Resolves when the port replies on EP3 for this txn.
   * Silence on EP3 = port control firmware not answering (ATTACH/LIST/CONNECT).
   */
  writeEP4(data: Uint8Array): Promise<[DataView, Uint8Array]>;
  /** Control plane ← port (IN): unsolicited SYSTEMS / CIRCUIT_* (txn replies handled inside writeEP4). */
  onEP3Received(callback: (header: DataView, payload: Uint8Array) => void): void;
}
