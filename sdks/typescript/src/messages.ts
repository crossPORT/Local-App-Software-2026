/**
 * Host↔port control types (EP4 out / EP3 in) — simulate Session LIST/CONNECT only.
 * Real HW + tunnel use EP4 switch + ROCKETBX data (no ATTACH).
 */

/** Host → port (EP4) */
export const MSG_LIST = 0x02;
export const MSG_CONNECT = 0x03;
export const MSG_DISCONNECT = 0x04;
export const MSG_KEEPALIVE = 0x05;

/** Port → host (EP3) */
export const MSG_SYSTEMS = 0x82;
export const MSG_ACK = 0x83;
export const MSG_NAK = 0x84;
export const MSG_CIRCUIT_UP = 0x85;
export const MSG_CIRCUIT_DOWN = 0x86;
