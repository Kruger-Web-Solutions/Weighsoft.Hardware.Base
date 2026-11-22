import { describe, expect, it } from '@jest/globals';

import {
  adWriteRequest, alarmReadRequest, calibrateBlock, calibratesLoadcell, digitsText, loadcellDivisions, loadcellTestWeight,
  parseTestWeight, spanDigits
} from './calibrate';
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

describe('junction box requests', () => {
  // The board answers bad_channel to an AD or alarm request without a channel.
  it('sends zero and span to load cell 1', () => {
    expect(adWriteRequest(0)).toEqual({ op: 'write', field: 'ad', value: 0, channel: 1 });
    expect(adWriteRequest(2000)).toEqual({ op: 'write', field: 'ad', value: 2000, channel: 1 });
  });

  it('reads the alarm bits of load cell 1', () => {
    expect(alarmReadRequest()).toEqual({ op: 'read', field: 'alarm', channel: 1 });
  });
});

describe('load cell on an HX711 board', () => {
  it('gets its own calibrate steps, but never on a Writer or another source', () => {
    expect(calibratesLoadcell(status({ source: 'loadcell' }))).toBe(true);
    // Not calibrated yet means no weight and no link: the steps must still show.
    expect(calibratesLoadcell(status({ source: 'loadcell', linked: false }))).toBe(true);
    expect(calibratesLoadcell(status({ source: 'loadcell', role: 'writer' }))).toBe(false);
    expect(calibratesLoadcell(status({ source: 'rs485' }))).toBe(false);
    expect(calibratesLoadcell(status({ source: 'rs232' }))).toBe(false);
  });

  it('shows digits with the point put back', () => {
    expect(digitsText(2000, 3)).toBe('2.000');
    expect(digitsText(5, 3)).toBe('0.005');
    expect(digitsText(-15, 2)).toBe('-0.15');
    expect(digitsText(250, 0)).toBe('250');
    expect(digitsText(0, 1)).toBe('0.0');
  });

  it('sends the test weight as typed, keeping its decimals', () => {
    expect(loadcellTestWeight('2.000')).toBe('2.000');
    expect(loadcellTestWeight(' 2,5 ')).toBe('2.5');
    expect(loadcellTestWeight('500')).toBe('500');
    expect(loadcellTestWeight('0')).toBeUndefined();
    expect(loadcellTestWeight('0.000')).toBeUndefined();
    expect(loadcellTestWeight('1.23456')).toBeUndefined();
    expect(loadcellTestWeight('-2')).toBeUndefined();
    expect(loadcellTestWeight('2kg')).toBeUndefined();
    expect(loadcellTestWeight('')).toBeUndefined();
  });
});

describe('load cell scale setup', () => {
  it('lists the divisions finest first, 1 2 5 in each digit', () => {
    const kg = loadcellDivisions('kg');
    expect(kg[0]).toEqual({ decimals: 4, step: 1, key: '4:1', text: '0.0001' });
    expect(kg.map((d) => d.text)).toEqual([
      '0.0001', '0.0002', '0.0005', '0.001', '0.002', '0.005', '0.01', '0.02', '0.05', '0.1', '0.2', '0.5',
      '1', '2', '5', '10', '20', '50'
    ]);
    // 0.01 g is the finest the board's setup rules can hold.
    expect(loadcellDivisions('g').map((d) => d.text)).toEqual([
      '0.01', '0.02', '0.05', '0.1', '0.2', '0.5', '1', '2', '5', '10', '20', '50'
    ]);
  });
});
