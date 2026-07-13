import { RocketBoxTransportImpl } from './rocketbox_transport';
import type { RocketBoxTransport } from './types';

export type CreateRocketBoxTransportOptions = {
  simulate?: boolean;
  /** Port 1–4 (sim and display); default 1. */
  port?: number;
};

/** One implementation; simulate selects HwPlane vs SimTransport. */
export function createRocketBoxTransport(
  options: CreateRocketBoxTransportOptions = {},
): RocketBoxTransport {
  return new RocketBoxTransportImpl(options.port ?? 1, Boolean(options.simulate));
}
