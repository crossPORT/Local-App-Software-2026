/** RocketBox IntelliConnex USB identity and default 4-endpoint layout. */
export const VENDOR_ID = 0x1772;
export const PRODUCT_ID = 0x0006;
export const INTERFACE_NUMBER = 0;

/** Logical EP → WebUSB endpointNumber (direction separate). */
export const DEFAULT_EP1_OUT = 1;
export const DEFAULT_EP2_IN = 2;
export const DEFAULT_EP3_IN = 3;
export const DEFAULT_EP4_OUT = 4;

export const USB_READ_SIZE = 16 * 1024;
export const MSG_NAK = 0x84;

export interface UsbEndpoints {
  ep1Out: number;
  ep2In: number;
  ep3In: number;
  ep4Out: number;
}

export const DEFAULT_ENDPOINTS: UsbEndpoints = {
  ep1Out: DEFAULT_EP1_OUT,
  ep2In: DEFAULT_EP2_IN,
  ep3In: DEFAULT_EP3_IN,
  ep4Out: DEFAULT_EP4_OUT,
};
