import { RocketBoxTransport } from './rocketbox_transport';
import type { FabricTransport } from './types';

export type CreateFabricTransportOptions = {
  simulate?: boolean;
  /** Fabric port 1–4 (sim and display); default 1. */
  port?: number;
};

/** One implementation: RocketBoxTransport; simulate selects UsbTransport vs SimTransport. */
export function createFabricTransport(
  options: CreateFabricTransportOptions = {},
): FabricTransport {
  return new RocketBoxTransport(options.port ?? 1, Boolean(options.simulate));
}
