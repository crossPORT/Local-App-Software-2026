export type { ReceiveStatus, AnnounceIdentity } from './identity';
export { FabricUsbError, RocketBoxError } from './errors';
export {
  FABRIC_LEG_COUNT,
  PORT_COUNT,
  displayPortFromLeg,
  toDisplayPort,
  legFromDisplayPort,
  toPortIndex,
  legFromWirePort,
  wirePortFromLeg,
  fabricLegFromSerial,
  resolveFabricLegFromDevice,
  fabricPortFromSerial,
  sortFabricDevicesBySerial,
  remoteFabricLegs,
  formatFabricLegLabel,
  formatFabricPortDisplay,
  formatPortLabel,
} from './port';
export {
  VENDOR_ID,
  PRODUCT_ID,
  INTERFACE_NUMBER,
  EP_OUT,
  EP_IN,
  HEADER_SIZE,
  CHUNK_SIZE,
  HEADER_MAGIC,
  buildHeader,
  parseHeader,
  concatChunks,
  boothDisplayMibSWithJitter,
  rollBoothDisplayMibS,
  rollDisplayMibS,
  type ParsedHeader,
} from './protocol';
export {
  FRAME_KIND_SESSION,
  FRAME_KIND_PAYLOAD,
  FRAME_KIND_OFFSET,
  FRAME_FILENAME_OFFSET,
  FRAME_FILENAME_MAX,
  frameKindToWire,
  wireToFrameKind,
  type WireFrameKind,
} from './frame';
export { MAX_SESSION_FILE_BYTES } from './limits';
export {
  serializeSessionMessage,
  parseSessionPayload,
  buildAnnounceMessage,
  buildSessionReply,
} from './session_codec';
export {
  type FabricSessionMessage,
  type SessionMessage,
  type SessionMessageKind,
  sessionKindFromString,
  sessionKindToString,
  makeSessionId,
  makeInstanceId,
} from './session_types';
export {
  buildAnnounceNote,
  parseAnnounceNote,
  resolveRemoteFabricLeg,
  resolveRemoteFabricPort,
  type ParsedAnnounceNote,
} from './announce_note';
export type { RocketBoxTransport, FabricTransport } from './transport';
export type { ListenMode, FabricLinkEvent } from './link_types';
export { FabricLink, clearEndpointHalts } from './fabric_link';
export { FabricUsbSession } from './usb_session';
export { webUsbBlockedReason } from './webusb_avail';
export {
  createRocketBoxTransport,
  createRocketBoxUsbTransport,
  countUsbDevices,
  usbHasSavedSerial,
  clearUsbSavedPairing,
} from './create_transport';
export { setSdkLogSink, setSdkLogLevel, boothLog, getBoothLogLevel } from './debug_log';
export { hasSavedSerial, clearSavedSerial as clearSavedUsbPairing } from './usb_pairing';

export async function readFilePayload(file: File): Promise<Uint8Array> {
  const buffer = await file.arrayBuffer();
  return new Uint8Array(buffer);
}
