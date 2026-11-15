import { describe, expect, it } from '@jest/globals';

import { passwordProblem, wifiUpdateChip } from './wifiUpdate';
import { WSoftWifiUpdate } from './types';

const info = (over: Partial<WSoftWifiUpdate>): WSoftWifiUpdate => ({
  possible: true,
  on: false,
  password_set: false,
  approval_on: false,
  listening: false,
  blocked: null,
  port: 3232,
  password_min: 12,
  password_max: 64,
  version: '0.7.0',
  ...over
});

describe('passwordProblem', () => {
  it('says nothing while the box is empty', () => {
    expect(passwordProblem('', 12, 64)).toBeUndefined();
  });
  it('wants 12 to 64 characters', () => {
    expect(passwordProblem('elevenchars', 12, 64)).toBe('At least 12 characters');
    expect(passwordProblem('twelvechars!', 12, 64)).toBeUndefined();
    expect(passwordProblem('a'.repeat(65), 12, 64)).toBe('At most 64 characters');
  });
  it('refuses spaces', () => {
    expect(passwordProblem('has a space in it', 12, 64)).toMatch(/No spaces/);
  });
});

describe('wifiUpdateChip', () => {
  it('shows cable only on a one-copy board, whatever else is set', () => {
    expect(wifiUpdateChip(info({ possible: false, on: true, blocked: 'ONE_COPY' })).label).toBe('Cable only');
  });
  it('shows OFF by default', () => {
    expect(wifiUpdateChip(info({})).label).toBe('OFF');
  });
  it('shows the Approval lock', () => {
    const chip = wifiUpdateChip(info({ on: true, password_set: true, approval_on: true, blocked: 'APPROVAL_ON' }));
    expect(chip.label).toBe('ON, locked: Approval is ON');
    expect(chip.color).toBe('warning');
  });
  it('shows ready only when the board is listening', () => {
    expect(wifiUpdateChip(info({ on: true, password_set: true, listening: true })).color).toBe('success');
    expect(wifiUpdateChip(info({ on: true, password_set: true })).label).toBe('ON, waiting for Wi-Fi');
  });
});
