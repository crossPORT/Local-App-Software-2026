export type RocketBoxErrorCode =
  | 'usb'
  | 'pairing'
  | 'protocol'
  | 'timeout'
  | 'busy'
  | 'unavailable';

export class RocketBoxError extends Error {
  readonly code: RocketBoxErrorCode;

  constructor(message: string, code: RocketBoxErrorCode = 'usb') {
    super(message);
    this.name = 'RocketBoxError';
    this.code = code;
  }
}
