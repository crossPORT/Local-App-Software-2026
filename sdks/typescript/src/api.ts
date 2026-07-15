/** Public RocketBox surface for @rocketbox/sdk consumers. */
export {
  buildAnnounceMessage,
  buildAnnounceNote,
  receiveStatusFromAnnounceNote,
} from './announce_note';
export { createRocketBoxTransport, type CreateRocketBoxTransportOptions } from './create_transport';
export type {
  RocketBoxTransport,
  RocketBoxLink,
  RocketBoxDataPlane,
  RocketBoxSessionPlane,
  ListenMode,
  AnnouncePresenceMode,
} from './types';
export { RocketBoxError, type RocketBoxErrorCode } from './errors';
export {
  PORT_COUNT,
  toDisplayPort,
  toPortIndex,
  portIndexFromWire,
  wirePortFromIndex,
  portIndexFromSerial,
  resolvePortIndexFromDevice,
  remotePortIndexes,
  remoteDisplayPorts,
  defaultPairDisplayPort,
  formatPortLabel,
} from './port';
export {
  buildSessionReply,
  parseSessionPayload,
  serializeSessionMessage,
  makeInstanceId,
  makeSessionId,
  sessionKindFromString,
  sessionKindToString,
  type SessionMessage,
  type SessionMessageKind,
} from './session_codec';
export type { ReceiveStatus } from './identity';
export {
  rollDisplayMibS,
  displayMibSWithJitter,
  buildHeader,
  parseHeader,
  type ParsedHeader,
} from './protocol';
export { isWebUsbAvailable, webUsbBlockedReason } from './transports/webusb_avail';
export {
  clearSavedUsbPairing,
  countDevices,
  hasSavedSerial,
} from './transports/usb_pairing';
export { setDebugLog, setDebugLevel, type DebugLevel } from './debug_log';
export { subscribeUsbDisconnect } from './transports/usb_events';
export {
  setEp4DynamicSwitchEnabled,
  isEp4DynamicSwitchEnabled,
} from './transports/usb_switch';
