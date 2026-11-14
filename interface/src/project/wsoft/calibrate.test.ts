import { describe, expect, it } from '@jest/globals';

import { calibrateBlock, parseTestWeight, spanDigits } from './calibrate';
import { WSoftStatus } from './types';

const status = (overrides: Partial<WSoftStatus>): WSoftStatus =>
  ({ linked: true, approval_on: false, source: 'rs485', role: 'weigh', decimals: 3, ...overrides } as WSoftStatus);

describe('test weight', () => {
  it('goes to the junction box in display digits', () => {
    expect(spanDigits(2, 3)).toBe(2000);
    expect(spanDigits(20.5, 1)).toBe(205);
    expect(spanDigits(1500, 0)).toBe(1500);
    expect(spanDigits(0.1, 3)).toBe(100);
  });

  it('reads 20, 20.5 and 20,5; refuses 0, empty and text', () => {
    expect(parseTestWeight('20')).toBe(20);
    expect(parseTestWeight(' 20.5 ')).toBe(20.5);
    expect(parseTestWeight('20,5')).toBe(20.5);
    expect(parseTestWeight('0')).toBeUndefined();
    expect(parseTestWeight('')).toBeUndefined();
    expect(parseTestWeight('-5')).toBeUndefined();
    expect(parseTestWeight('20kg')).toBeUndefined();
  });
});

describe('who can calibrate here', () => {
  it('a weigh board on the RS485 junction box, Approval off, linked', () => {
    expect(calibrateBlock(status({}))).toBeUndefined();
  });

  it('never a Writer or an RS232 scale, and not with Approval on or no link', () => {
    expect(calibrateBlock(status({ role: 'writer' }))).toBe('writer');
    expect(calibrateBlock(status({ source: 'rs232' }))).toBe('rs232');
    expect(calibrateBlock(status({ approval_on: true }))).toBe('approval');
    expect(calibrateBlock(status({ linked: false }))).toBe('not_linked');
  });
});
