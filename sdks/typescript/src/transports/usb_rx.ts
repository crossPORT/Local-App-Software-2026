import { MSG_NAK } from '../messages';

export type UsbTxnCb = {
  resolve: (header: DataView, payload: Uint8Array) => void;
  reject: (err: Error) => void;
};

export function concatBytes(a: Uint8Array, b: Uint8Array): Uint8Array {
  const out = new Uint8Array(a.length + b.length);
  out.set(a, 0);
  out.set(b, a.length);
  return out;
}

export function nakReason(arg: number): string {
  if (arg === 0x02) return 'offline';
  if (arg === 0x03) return 'denied';
  if (arg === 0x04) return 'invalid';
  if (arg === 0x05) return 'timeout';
  return 'busy';
}

export function drainEp2(
  buf: Uint8Array,
  onData: ((data: Uint8Array) => void) | null,
): Uint8Array {
  let cur = buf;
  while (cur.length >= 4) {
    const len = new DataView(cur.buffer, cur.byteOffset, 4).getUint32(0);
    if (cur.length < 4 + len) return cur;
    onData?.(cur.subarray(4, 4 + len));
    cur = cur.subarray(4 + len);
  }
  return cur;
}

export function drainEp3(
  buf: Uint8Array,
  txnCallbacks: Map<number, UsbTxnCb>,
  onControl: ((header: DataView, payload: Uint8Array) => void) | null,
): Uint8Array {
  let cur = buf;
  while (cur.length >= 12) {
    const header = new DataView(cur.buffer, cur.byteOffset, 12);
    const payloadLen = header.getUint32(8);
    if (cur.length < 12 + payloadLen) return cur;
    const payload = cur.subarray(12, 12 + payloadLen);
    const hdrCopy = new DataView(cur.slice(0, 12).buffer);
    cur = cur.subarray(12 + payloadLen);
    dispatchControl(hdrCopy, payload, txnCallbacks, onControl);
  }
  return cur;
}

function dispatchControl(
  header: DataView,
  payload: Uint8Array,
  txnCallbacks: Map<number, UsbTxnCb>,
  onControl: ((header: DataView, payload: Uint8Array) => void) | null,
): void {
  const txn = header.getUint16(2);
  const type = header.getUint8(1);
  const arg = header.getUint32(4);
  const cb = txnCallbacks.get(txn);
  if (cb) {
    txnCallbacks.delete(txn);
    if (type === MSG_NAK) cb.reject(new Error(nakReason(arg)));
    else cb.resolve(header, payload);
    return;
  }
  onControl?.(header, payload);
}
