import { describe, expect, it } from 'vitest';
import { RocketBoxError } from '@rocketbox/sdk';
import { formatTransferError, formatUsbConnectError, isUsbPickerCancel } from './user_errors';

describe('isUsbPickerCancel', () => {
  it('detects NotFoundError and picker cancel messages', () => {
    expect(isUsbPickerCancel(new DOMException('No device selected', 'NotFoundError'))).toBe(true);
    expect(isUsbPickerCancel(new Error('No device selected'))).toBe(true);
    expect(isUsbPickerCancel(new Error('Access denied'))).toBe(false);
  });
});

describe('formatUsbConnectError', () => {
  it('returns null for picker cancel', () => {
    expect(formatUsbConnectError(new DOMException('', 'NotFoundError'))).toBeNull();
  });

  it('passes through RocketBoxError message', () => {
    expect(formatUsbConnectError(new RocketBoxError('interface is busy', 'busy'))).toBe(
      'interface is busy',
    );
  });

  it('maps common WebUSB failures to user copy', () => {
    expect(formatUsbConnectError(new Error('WebUSB requires HTTPS'))).toMatch(/HTTPS/);
    expect(formatUsbConnectError(new Error('SecurityError Access denied'))).toMatch(/access denied/i);
    expect(formatUsbConnectError(new Error('device disconnected'))).toMatch(/disconnected/i);
    expect(formatUsbConnectError(new Error('No USB device found'))).toMatch(/Cable not found/);
    expect(formatUsbConnectError(new Error('timeout'))).toMatch(/ATTACH|EP3|firmware/i);
    expect(
      formatUsbConnectError(
        new RocketBoxError(
          'USB opened, but the local port did not reply on EP3 after ATTACH — port control firmware required (not just 4 endpoints).',
          'timeout',
        ),
      ),
    ).toMatch(/did not reply on EP3/i);
  });
});

describe('formatTransferError', () => {
  it('maps disconnect during transfer', () => {
    expect(formatTransferError(new Error('device disconnected'))).toBe(
      'USB disconnected during transfer.',
    );
  });

  it('falls back to generic message', () => {
    expect(formatTransferError(new Error(''))).toBe('Transfer failed — try again.');
  });
});
