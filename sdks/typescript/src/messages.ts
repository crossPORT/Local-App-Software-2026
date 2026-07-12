/**
 * Host↔port control types (EP4 out / EP3 in) — used by simulate Session path only.
 * Real HW uses port switch on EP4 + ROCKETBX transfer on data endpoints (see AGENTS.md).
 */

/** Host → port (EP4) */
export const MSG_ATTACH = 0x01;
export const MSG_LIST = 0x02;
export const MSG_CONNECT = 0x03;
export const MSG_DISCONNECT = 0x04;
export const MSG_KEEPALIVE = 0x05;

/** Port → host (EP3) */
export const MSG_ATTACHED = 0x81;
export const MSG_SYSTEMS = 0x82;
export const MSG_ACK = 0x83;
export const MSG_NAK = 0x84;
export const MSG_CIRCUIT_UP = 0x85;
export const MSG_CIRCUIT_DOWN = 0x86;
