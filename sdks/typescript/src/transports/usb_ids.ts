/** RocketBox USB identity and logical 4-endpoint layout (host view). */
export const VENDOR_ID = 0x1772;
export const PRODUCT_ID = 0x0006;
export const INTERFACE_NUMBER = 0;

/**
 * Logical endpoints (may remap if firmware uses 0x02/0x81 for data — see discoverEndpoints).
 * EP1/EP2 → crossport fabric data. EP3/EP4 → host↔port control (fabric never touches).
 */
export const DEFAULT_EP1_OUT = 1;
export const DEFAULT_EP2_IN = 2;
export const DEFAULT_EP3_IN = 3;
export const DEFAULT_EP4_OUT = 4;

export const USB_READ_SIZE = 16 * 1024;

export interface UsbEndpoints {
  /** Data OUT into fabric circuit. */
  ep1Out: number;
  /** Data IN from fabric circuit. */
  ep2In: number;
  /** Control IN from local port. */
  ep3In: number;
  /** Control OUT to local port. */
  ep4Out: number;
}

export const DEFAULT_ENDPOINTS: UsbEndpoints = {
  ep1Out: DEFAULT_EP1_OUT,
  ep2In: DEFAULT_EP2_IN,
  ep3In: DEFAULT_EP3_IN,
  ep4Out: DEFAULT_EP4_OUT,
};
