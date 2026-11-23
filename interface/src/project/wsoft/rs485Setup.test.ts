import { describe, expect, it } from '@jest/globals';

import {
  boardValues, cardWrites, eStep, formTuning, onCard, rowCheck, rs485Problems, Rs485Form, secondsToTenths, tenthsToSeconds,
  toRs485Form, typedValue, valueText
} from './rs485Setup';

const form = (overrides: Partial<Rs485Form> = {}): Rs485Form => ({
  unit: '1',
  decimal: '2',
  e_kg: '0.02',
  stable_range: '2',
  stable_time: '0.5',
  zero_track_range: '1',
  zero_track_time: '1.0',
  filter1: '3',
  filter2: '4',
  ...overrides
});

describe('seconds and tenths', () => {
  it('reads seconds as tenths, the way the card keeps them', () => {
    expect(secondsToTenths('0.5')).toBe(5);
    expect(secondsToTenths('9.9')).toBe(99);
    expect(secondsToTenths('1')).toBe(10);
    expect(secondsToTenths('0,3 s')).toBe(3);
    expect(secondsToTenths('0')).toBe(0);
    expect(tenthsToSeconds(5)).toBe('0.5');
  });

  it('refuses more than 9.9 s, hundredths and text', () => {
    expect(secondsToTenths('10')).toBeUndefined();
    expect(secondsToTenths('0.25')).toBeUndefined();
    expect(secondsToTenths('soon')).toBeUndefined();
    expect(secondsToTenths('')).toBeUndefined();
  });
});

describe('e', () => {
  it('is a card step of 1, 2 or 5 at the decimal places', () => {
    expect(eStep('2', '0.02')).toBe(2);
    expect(eStep('0', '5')).toBe(5);
    expect(eStep('3', '0.5')).toBe(500);
    expect(eStep('4', '0.0001')).toBe(1);
  });

  it('refuses a step the card does not take', () => {
    expect(eStep('2', '0.03')).toBeUndefined();
    expect(eStep('2', '0.005')).toBeUndefined();
    expect(eStep('', '1')).toBeUndefined();
    expect(eStep('0', '1000')).toBeUndefined();
  });
});

describe('the form', () => {
  it('fills from the board and the filters read from the card', () => {
    const filled = toRs485Form({ division: 2, decimal: 2, unit: 1 },
      { stable_range: 3, stable_time: 5, zero_track_range: 2, zero_track_time: 10 }, { filter1: 4 });
    expect(filled).toEqual(form({ stable_range: '3', zero_track_range: '2', filter1: '4', filter2: '' }));
    expect(formTuning(filled)).toEqual({ stable_range: 3, stable_time: 5, zero_track_range: 2, zero_track_time: 10 });
  });

  it('checks every field and leaves an unread filter alone', () => {
    expect(rs485Problems(form())).toEqual({});
    expect(rs485Problems(form({ filter1: '', filter2: '' }))).toEqual({});
    expect(Object.keys(rs485Problems(form({
      stable_range: '1000', stable_time: '12', zero_track_range: 'x', zero_track_time: '0', filter1: '10', filter2: '11', e_kg: '0.03'
    }))).sort()).toEqual(['e_kg', 'filter1', 'filter2', 'stable_range', 'stable_time', 'zero_track_range', 'zero_track_time']);
  });
});

describe('where each value is written', () => {
  it('writes everything to a DJB-M1X, unit and e first', () => {
    expect(cardWrites(form(), false, {}).map((w) => [w.field, w.value])).toEqual([
      ['unit', 1], ['decimal', 2], ['division', 2], ['stability_range', 2], ['stability_time', 5],
      ['zero_track_limit', 1], ['zero_track_time', 10], ['filter1', 3], ['filter2', 4]
    ]);
  });

  it('writes only the filters to a DJB-W1X: the board keeps the rest', () => {
    expect(cardWrites(form(), true, {}).map((w) => w.field)).toEqual(['filter1', 'filter2']);
    expect(onCard('division', true)).toBe(false);
    expect(onCard('filter2', true)).toBe(true);
  });

  it('skips what the card already holds, an unread filter and stable time off', () => {
    const writes = cardWrites(form({ stable_time: '0', filter2: '' }), false,
      { unit: 1, decimal: 2, division: 2, stability_range: 2, zero_track_limit: 1, zero_track_time: 10, filter1: 3 });
    expect(writes).toEqual([]);
  });
});

describe('the check table', () => {
  const board = boardValues({ division: 2, decimal: 2, unit: 1 },
    { stable_range: 2, stable_time: 5, zero_track_range: 1, zero_track_time: 10 });

  it('says when the card holds what is typed', () => {
    expect(rowCheck('filter1', form(), true, { filter1: 3 }, board)).toBe('same');
    expect(rowCheck('filter1', form(), true, { filter1: 0 }, board)).toBe('differs');
    expect(rowCheck('filter2', form(), true, {}, board)).toBe('unread');
    expect(rowCheck('stability_range', form(), false, { stability_range: 2 }, board)).toBe('same');
  });

  it('says when the board keeps it (DJB-W1X), and when it is not saved yet', () => {
    expect(rowCheck('division', form(), true, { division: 0 }, board)).toBe('board');
    expect(rowCheck('stability_time', form({ stable_time: '0.8' }), true, {}, board)).toBe('board_differs');
  });

  it('shows card numbers the way a person reads them', () => {
    expect(valueText('unit', 3)).toBe('g');
    expect(valueText('division', 2, 2)).toBe('0.02');
    expect(valueText('stability_time', 5)).toBe('0.5 s');
    expect(valueText('filter1', undefined)).toBe('-');
    expect(typedValue(form(), 'zero_track_time')).toBe(10);
  });
});
