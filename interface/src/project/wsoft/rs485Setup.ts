// One Scale setup for the RS485 weighing card (Rhyno, 23 Nov 2026): the values typed at the top of
// the Scale setup tab are written where they live, and the check table under it reads them back.
//
// Where each value lives (lib/wsoft_core/src/wsoft_rs485_tuning.h):
//   - DJB-M1X (LED display): every value is a card register; the board writes it and reads it back.
//   - DJB-W1X (no display): unit, decimal places, e, stable and auto zero have no register on the
//     card (its manual lists none; on the bench it answered 0 and kept no write), so this board
//     keeps them. Filter 1 and Filter 2 are in protocol V2.0 for both cards, so they go to the card.
// Pure functions only, so the page and the tests share them (rs485Setup.test.ts).

import { WSoftCardField, WSoftRs485Tuning, WSoftScaleCard } from './types';

/** Everything the RS485 part of the form holds, as typed text. Times are in seconds. */
export interface Rs485Form {
  unit: string;
  decimal: string;
  e_kg: string;
  stable_range: string;
  stable_time: string;
  zero_track_range: string;
  zero_track_time: string;
  filter1: string;
  filter2: string;
}

/** Values read from the card, in its own numbers (register values). */
export type CardValues = Partial<Record<WSoftCardField, number>>;

export const UNITS: { value: number; text: string }[] = [
  { value: 1, text: 'kg' },
  { value: 2, text: 't' },
  { value: 3, text: 'g' }
];

// Card limits (M1X manual menu 05..08, protocol V2.0 41321 / 41323). Same as lib/wsoft_core/src/wsoft_djb.cpp.
export const STABLE_RANGE_MAX = 999;
export const ZERO_TRACK_MAX = 9999;
export const FILTER1_MAX = 9;
export const FILTER2_MAX = 10;
const DIVISION_STEPS = [1, 2, 5, 10, 20, 50, 100, 200, 500];

const WHOLE = /^[0-9]+$/;
const KG = /^[0-9]+(\.[0-9]{1,5})?$/;
const clean = (value: string) => value.trim().replace(/\s*(kg|s)$/i, '').replace(',', '.');

/** "0.5" seconds as 5 tenths. Undefined when not 0.0 to 9.9 in tenths. */
export const secondsToTenths = (value: string): number | undefined => {
  const text = clean(value);
  if (!/^[0-9](\.[0-9])?$/.test(text)) return undefined;
  return Math.round(Number(text) * 10);
};

/** 5 tenths as "0.5". */
export const tenthsToSeconds = (tenths: number) => (tenths / 10).toFixed(1);

/** The card step for e at these decimal places (e 0.02 kg with 2 decimals = 2), or undefined. */
export const eStep = (decimal: string, eKg: string): number | undefined => {
  const places = Number(decimal);
  const text = clean(eKg);
  if (decimal === '' || !KG.test(text) || Number(text) <= 0) return undefined;
  const step = Math.round(Number(text) * 10 ** places);
  if (Math.abs(step / 10 ** places - Number(text)) > 1e-9) return undefined;
  return DIVISION_STEPS.includes(step) ? step : undefined;
};

/** e in kg from the card's step and decimal places: 2 at 2 places is "0.02". */
export const eText = (division: number, decimal: number) => (division / 10 ** decimal).toFixed(decimal);

/** The form from what the board sends (card = the format in use) and the filters read from the card. */
export const toRs485Form = (card: WSoftScaleCard | null, tuning: WSoftRs485Tuning | undefined, filters: CardValues): Rs485Form => ({
  unit: card && card.unit > 0 ? String(card.unit) : '1',
  decimal: card ? String(card.decimal) : '',
  e_kg: card && card.division > 0 ? eText(card.division, card.decimal) : '',
  stable_range: tuning ? String(tuning.stable_range) : '0',
  stable_time: tuning ? tenthsToSeconds(tuning.stable_time) : '0.0',
  zero_track_range: tuning ? String(tuning.zero_track_range) : '0',
  zero_track_time: tuning ? tenthsToSeconds(tuning.zero_track_time) : '1.0',
  filter1: filters.filter1 === undefined ? '' : String(filters.filter1),
  filter2: filters.filter2 === undefined ? '' : String(filters.filter2)
});

const whole = (value: string, max: number, blankOk = false) => {
  const text = value.trim();
  if (text === '') return blankOk ? undefined : 'Needed';
  if (!WHOLE.test(text) || Number(text) > max) return `A whole number, 0 to ${max}`;
  return undefined;
};

const seconds = (value: string, min: number) => {
  const tenths = secondsToTenths(value);
  if (tenths === undefined || tenths < min) return `Seconds, ${tenthsToSeconds(min)} to 9.9`;
  return undefined;
};

/** One plain sentence per field that is wrong; an empty object when the form can be saved. */
export const rs485Problems = (form: Rs485Form): Partial<Record<keyof Rs485Form, string>> => {
  const problems: Partial<Record<keyof Rs485Form, string>> = {
    stable_range: whole(form.stable_range, STABLE_RANGE_MAX),
    stable_time: seconds(form.stable_time, 0),
    zero_track_range: whole(form.zero_track_range, ZERO_TRACK_MAX),
    zero_track_time: seconds(form.zero_track_time, 1),
    // Empty = not read from the card yet: left as it is.
    filter1: whole(form.filter1, FILTER1_MAX, true),
    filter2: whole(form.filter2, FILTER2_MAX, true)
  };
  if (eStep(form.decimal, form.e_kg) === undefined) {
    problems.e_kg = 'Pick the decimal places, then e as 1, 2 or 5 of a digit (like 0.02 with 2 places)';
  }
  Object.keys(problems).forEach((key) => {
    if (!problems[key as keyof Rs485Form]) delete problems[key as keyof Rs485Form];
  });
  return problems;
};

/** The board's tuning from the form (times in tenths). */
export const formTuning = (form: Rs485Form): WSoftRs485Tuning => ({
  stable_range: Number(form.stable_range.trim()),
  stable_time: secondsToTenths(form.stable_time) ?? 0,
  zero_track_range: Number(form.zero_track_range.trim()),
  zero_track_time: secondsToTenths(form.zero_track_time) ?? 10
});

/** The rows of the check table, in the order the card is written (unit and e first). */
export const CHECK_ROWS: { field: WSoftCardField; label: string }[] = [
  { field: 'unit', label: 'Unit' },
  { field: 'decimal', label: 'Decimal places' },
  { field: 'division', label: 'e' },
  { field: 'stability_range', label: 'Stable range' },
  { field: 'stability_time', label: 'Stable time' },
  { field: 'zero_track_limit', label: 'Auto zero range' },
  { field: 'zero_track_time', label: 'Auto zero time' },
  { field: 'filter1', label: 'Filter 1' },
  { field: 'filter2', label: 'Filter 2' }
];

const FILTERS: WSoftCardField[] = ['filter1', 'filter2'];

/** A DJB-W1X has only the filters: the rest is kept on this board. A DJB-M1X keeps everything. */
export const onCard = (field: WSoftCardField, formatOnBoard: boolean) => !formatOnBoard || FILTERS.includes(field);

/** The typed value as the card's register value, or undefined when there is nothing to write. */
export const typedValue = (form: Rs485Form, field: WSoftCardField): number | undefined => {
  const wholeOf = (text: string) => (WHOLE.test(text.trim()) ? Number(text.trim()) : undefined);
  switch (field) {
    case 'unit': return wholeOf(form.unit);
    case 'decimal': return wholeOf(form.decimal);
    case 'division': return eStep(form.decimal, form.e_kg);
    case 'stability_range': return wholeOf(form.stable_range);
    case 'stability_time': return secondsToTenths(form.stable_time);
    case 'zero_track_limit': return wholeOf(form.zero_track_range);
    case 'zero_track_time': return secondsToTenths(form.zero_track_time);
    case 'filter1': return wholeOf(form.filter1);
    case 'filter2': return wholeOf(form.filter2);
    default: return undefined;
  }
};

/**
 * The card writes for a Save, in order, leaving out a value the card already holds. Stable time 0
 * (the board's "off") is not written: the card takes 0.1 to 9.9 s.
 */
export const cardWrites = (form: Rs485Form, formatOnBoard: boolean, card: CardValues) =>
  CHECK_ROWS
    .filter((row) => onCard(row.field, formatOnBoard))
    .map((row) => ({ field: row.field, value: typedValue(form, row.field) }))
    .filter((write): write is { field: WSoftCardField; value: number } =>
      write.value !== undefined && !(write.field === 'stability_time' && write.value === 0) && card[write.field] !== write.value);

/** A value as a person reads it: unit as text, e in kg, times in seconds. */
export const valueText = (field: WSoftCardField, value: number | undefined, decimal?: number) => {
  if (value === undefined) return '-';
  switch (field) {
    case 'unit': return UNITS.find((unit) => unit.value === value)?.text ?? `code ${value}`;
    case 'division': return decimal === undefined ? `step ${value}` : eText(value, decimal);
    case 'stability_time':
    case 'zero_track_time': return `${tenthsToSeconds(value)} s`;
    default: return String(value);
  }
};

export type CheckState = 'same' | 'differs' | 'board' | 'board_differs' | 'unread';

/**
 * The check for one row. On the card: does the card hold what is typed? Kept on the board: does the
 * board hold it (board = the values the board sent back after the last save)?
 */
export const rowCheck = (field: WSoftCardField, form: Rs485Form, formatOnBoard: boolean, card: CardValues,
  board: CardValues): CheckState => {
  const typed = typedValue(form, field);
  if (!onCard(field, formatOnBoard)) return board[field] === typed ? 'board' : 'board_differs';
  if (card[field] === undefined) return 'unread';
  if (field === 'stability_time' && typed === 0) return 'board';
  return card[field] === typed ? 'same' : 'differs';
};

export const CHECK_TEXT: Record<CheckState, string> = {
  same: '✓ Saved on the card',
  differs: '✗ The card holds something else',
  board: '✓ Kept on this board',
  board_differs: '✗ Not saved yet: press Save',
  unread: 'Press Read'
};

/** The board's own copy (format kept for a W1X, tuning) in card numbers, for rowCheck. */
export const boardValues = (card: WSoftScaleCard | null, tuning: WSoftRs485Tuning | undefined): CardValues => ({
  unit: card?.unit,
  decimal: card?.decimal,
  division: card?.division,
  stability_range: tuning?.stable_range,
  stability_time: tuning?.stable_time,
  zero_track_limit: tuning?.zero_track_range,
  zero_track_time: tuning?.zero_track_time
});
