import { describe, expect, it } from '@jest/globals';

import {
  adWriteRequest, alarmReadRequest, calibrateBlock, calibratesLoadcell, digitsText, loadcellDivisions, parseTestWeight,
  LOADCELL_AUTO_ZERO, LOADCELL_DRIFT_GUARD, LOADCELL_FILTERS, noiseHint, noiseText
} from './calibrate';
import { WSoftStatus } from './types';

const status = (overrides: Partial<WSoftStatus>): WSoftStatus =>
  ({ linked: true, approval_on: false, source: 'rs485', role: 'weigh', decimals: 3, ...overrides } as WSoftStatus);

describe('test weight', () => {
  it('is read one way for the junction box and the load cell', () => {
    // text goes to the load cell board, digits to the junction box.
    expect(parseTestWeight('20', 3)).toEqual({ text: '20', digits: 20000 });
    expect(parseTestWeight(' 20.5 ', 1)).toEqual({ text: '20.5', digits: 205 });
    expect(parseTestWeight('20,5', 3)).toEqual({ text: '20.5', digits: 20500 });
    expect(parseTestWeight('2.000', 3)).toEqual({ text: '2.000', digits: 2000 });
    expect(parseTestWeight('1500', 0)).toEqual({ text: '1500', digits: 1500 });
    expect(parseTestWeight('0.1', 3)).toEqual({ text: '0.1', digits: 100 });
    // Exact: 0.29 * 100 is 28.999... as a float.
    expect(parseTestWeight('0.29', 2)).toEqual({ text: '0.29', digits: 29 });
  });

  it('refuses 0, empty, text and a negative', () => {
    expect(parseTestWeight('0', 3)).toBeUndefined();
    expect(parseTestWeight('0.000', 3)).toBeUndefined();
    expect(parseTestWeight('', 3)).toBeUndefined();
    expect(parseTestWeight('-5', 3)).toBeUndefined();
    expect(parseTestWeight('20kg', 3)).toBeUndefined();
    expect(parseTestWeight('12345678', 3)).toBeUndefined();
  });

  it('refuses a weight finer than the scale shows, never rounds it', () => {
    expect(parseTestWeight('2.0005', 3)).toBeUndefined();
    expect(parseTestWeight('2.5', 0)).toBeUndefined();
    expect(parseTestWeight('2.0005', 4)).toEqual({ text: '2.0005', digits: 20005 });
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

  it('a load cell board whenever the HX711 answers, also before it shows a weight', () => {
    // Not calibrated yet means no weight and no link.
    expect(calibrateBlock(status({ source: 'loadcell', linked: false }), true)).toBeUndefined();
    expect(calibrateBlock(status({ source: 'loadcell', linked: false }))).toBeUndefined();
    expect(calibrateBlock(status({ source: 'loadcell' }), false)).toBe('no_cell');
    expect(calibrateBlock(status({ source: 'loadcell', approval_on: true }), true)).toBe('approval');
    expect(calibrateBlock(status({ source: 'loadcell', role: 'writer' }), true)).toBe('writer');
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
});

describe('load cell scale setup', () => {
  it('turns the raw noise into a plain hint', () => {
    expect(noiseText(4)).toBe('0.4');
    expect(noiseHint(null)).toBe('Steadies the weight. Higher is steadier and a little slower.');
    expect(noiseHint(undefined)).toBe(noiseHint(null));
    expect(noiseHint(4)).toBe('Noise now: 0.4 of a division. Low is enough.');
    expect(noiseHint(10)).toContain('Low is enough');
    expect(noiseHint(11)).toContain('Medium fits');
    expect(noiseHint(17)).toContain('Medium fits');
    expect(noiseHint(18)).toContain('High fits');
    expect(noiseHint(28)).toContain('High fits');
    expect(noiseHint(29)).toBe('Noise now: 2.9 of a division. Too much for a filter: pick a bigger division size.');
    // The four choices, numbered as the board numbers them.
    expect(LOADCELL_FILTERS.map((f) => f.value)).toEqual([0, 1, 2, 3]);
    // Auto zero: off, then the divisions around zero, as the board takes them.
    expect(LOADCELL_AUTO_ZERO.map((z) => z.value)).toEqual([0, 1, 2, 3]);
    expect(LOADCELL_AUTO_ZERO[0].text).toBe('Off');
    // Drift Guard: off, then the limit in divisions, as the board takes it (0..5).
    expect(LOADCELL_DRIFT_GUARD.map((d) => d.value)).toEqual([0, 1, 2, 3, 4, 5]);
    expect(LOADCELL_DRIFT_GUARD[0].text).toBe('Off');
  });

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
