/** Public fabric surface for @rocketbox/sdk consumers. */
export {
  buildAnnounceMessage,
  buildAnnounceNote,
  receiveStatusFromAnnounceNote,
} from './announce_note';
export { createFabricTransport, type CreateFabricTransportOptions } from './create_transport';
export type { FabricTransport, ListenMode } from './types';
export { FabricUsbError } from './errors';
export {
  FABRIC_LEG_COUNT,
  displayPortFromLeg,
  legFromDisplayPort,
  legFromWirePort,
  wirePortFromLeg,
  fabricLegFromSerial,
  resolveFabricLegFromDevice,
  remoteFabricLegs,
  formatFabricLegLabel,
  formatFabricPortDisplay,
} from './port';
export {
  buildSessionReply,
  parseSessionPayload,
  serializeSessionMessage,
  makeInstanceId,
  makeSessionId,
  sessionKindFromString,
  sessionKindToString,
  type FabricSessionMessage,
  type SessionMessageKind,
} from './session_codec';
export type { ReceiveStatus } from './identity';
export {
  rollBoothDisplayMibS,
  boothDisplayMibSWithJitter,
  buildHeader,
  parseHeader,
  type ParsedHeader,
} from './protocol';
export { isWebUsbAvailable, webUsbBlockedReason } from '../transports/webusb_avail';
export {
  clearSavedUsbPairing,
  countFabricDevices,
  hasSavedSerial,
} from '../transports/usb_pairing';
export { setFabricDebugLog, setFabricDebugLevel, type FabricDebugLevel } from './debug_log';
export { subscribeFabricUsbDisconnect } from '../transports/usb_events';
