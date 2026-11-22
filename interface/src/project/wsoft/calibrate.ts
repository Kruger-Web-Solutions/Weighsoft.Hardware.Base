// Calibrate tab rules (WSoftCalibrate.tsx), kept apart so host tests can pin them.
//
// The DJB junction box calibrates with its AD register (DJB-W/M1-16 Communication Protocol V2.0,
// LC1 AD Value 41003, command 10): write 0 = zero calibration with the scale empty; write the
// calibrating weight CW (> 0) = load calibration with that test weight on. CW is the weight as the
// box shows it, without the decimal point (2.000 kg at 3 decimals = 2000), as on the 2 Nov 2026
// Waveshare branch. The board refuses the write while Approval is ON, and counts every write
// that goes through on the audit counter.

import { WSoftDjbRequest, WSoftStatus } from './types';

/** The test weight in the box's display digits, e.g. 2.000 kg with 3 decimals = 2000. */
export const spanDigits = (weight: number, decimals: number) => Math.round(weight * Math.pow(10, decimals));

/** "20", "20.5" or "20,5" as a number above 0; undefined for anything else. */
export const parseTestWeight = (text: string) => {
  const clean = text.trim().replace(',', '.');
  if (!/^[0-9]+(\.[0-9]+)?$/.test(clean)) return undefined;
  const value = Number(clean);
  return value > 0 ? value : undefined;
};

/** Why this board can not calibrate from its web page right now, or undefined when it can. */
/** True when the Calibrate tab shows the load cell steps (WSoftLoadcellCalibrate) instead of the junction box ones. */
export const calibratesLoadcell = (status: WSoftStatus) => status.source === 'loadcell' && status.role !== 'writer';

/** Digits without the point back to text: 2000 at 3 decimals = "2.000". */
export const digitsText = (digits: number, decimals: number) => {
  const sign = digits < 0 ? '-' : '';
  const text = String(Math.abs(Math.trunc(digits))).padStart(decimals + 1, '0');
  return decimals > 0 ? `${sign}${text.slice(0, -decimals)}.${text.slice(-decimals)}` : `${sign}${text}`;
};

/** The test weight as the board takes it: "2", "2.000" or "2,5" (up to 4 decimals), above 0. Else undefined. */
export const loadcellTestWeight = (text: string) => {
  const clean = text.trim().replace(',', '.');
  if (!/^[0-9]{1,7}(\.[0-9]{1,4})?$/.test(clean)) return undefined;
  return Number(clean) > 0 ? clean : undefined;
};

// ---- Load cell: the unit and division choices on the Scale setup tab ----

export type LoadcellUnit = 'kg' | 'g';

/** One choice in the "Division size" list: 0.002 kg is decimals 3, step 2. */
export interface LoadcellDivision {
  decimals: number;
  step: number;
  /** "3:2", for the select. */
  key: string;
  /** "0.002" */
  text: string;
}

/** The filter choices, as the board numbers them (wsoft_loadcell.h). */
export const LOADCELL_FILTERS = [
  { value: 0, text: 'Off', detail: 'every reading as it comes' },
  { value: 1, text: 'Low', detail: 'fast' },
  { value: 2, text: 'Medium', detail: 'steady' },
  { value: 3, text: 'High', detail: 'steadiest, about 3 seconds to settle' }
];

/** Auto zero choices: how many divisions around zero the board follows a slow creep. */
export const LOADCELL_AUTO_ZERO = [
  { value: 0, text: 'Off' },
  { value: 1, text: 'On: within 1 division of zero' },
  { value: 2, text: 'On: within 2 divisions of zero' },
  { value: 3, text: 'On: within 3 divisions of zero' }
];

/** "0.4" divisions from the board's tenths. */
export const noiseText = (tenths: number) => (tenths / 10).toFixed(1);

/**
 * A plain hint from the raw noise. Our own rough estimate, not a measured rule: averaging N
 * counts shrinks the noise by about the square root of N, and the board holds the last digit
 * while the weight stays within a quarter of a division of the edge. So about 1 division of raw
 * noise is steady on Low (4 counts), 1.7 on Medium (12) and 2.8 on High (32).
 */
export const noiseHint = (tenths: number | null | undefined): string => {
  if (tenths === null || tenths === undefined) return 'Steadies the weight. Higher is steadier and a little slower.';
  const now = `Noise now: ${noiseText(tenths)} of a division.`;
  if (tenths <= 10) return `${now} Low is enough.`;
  if (tenths <= 17) return `${now} Medium fits.`;
  if (tenths <= 28) return `${now} High fits.`;
  return `${now} Too much for a filter: pick a bigger division size.`;
};

export const divisionKey = (decimals: number, step: number) => `${decimals}:${step}`;

/**
 * Every division the board takes for this unit, finest first: 1, 2 or 5 in each digit, then 10, 20
 * and 50 whole units. g stops at 0.01 g because the board's setup rules count in 0.00001 kg.
 */
export const loadcellDivisions = (unit: LoadcellUnit): LoadcellDivision[] => {
  const list: LoadcellDivision[] = [];
  for (let decimals = unit === 'g' ? 2 : 4; decimals >= 0; decimals--) {
    for (const step of decimals === 0 ? [1, 2, 5, 10, 20, 50] : [1, 2, 5]) {
      list.push({ decimals, step, key: divisionKey(decimals, step), text: digitsText(step, decimals) });
    }
  }
  return list;
};

export type CalibrateBlock = 'writer' | 'rs232' | 'approval' | 'not_linked';

export const calibrateBlock = (status: WSoftStatus): CalibrateBlock | undefined => {
  if (status.role === 'writer') return 'writer';
  if (status.source === 'rs232') return 'rs232';
  if (status.approval_on) return 'approval';
  if (!status.linked) return 'not_linked';
  return undefined;
};

export const CALIBRATE_BLOCK_TEXT: Record<CalibrateBlock, string> = {
  writer:
    'This board is a Writer: it shows the weigh board\'s weight and has no scale of its own. ' +
    'Calibrate on the weigh board\'s web page.',
  rs232:
    'This board reads an RS232 scale. That scale is calibrated on its own indicator (its keypad or its ' +
    'technician menu), not from here. The board only reads the weight it sends.',
  approval: 'Approval is ON. Switch Approval off on the Live tab before calibrating.',
  not_linked: 'The board can not see the junction box. Check the RS485 cable.'
};

// The junction box's first load cell (LC1). Its AD value is register 41003 in
// DJB protocol V2.0, and the board refuses an AD or alarm request without a channel.
export const LOAD_CELL_CHANNEL = 1;

// Write 0 for zero, or the span in display digits (DJB protocol V2.0, reg 41003).
export const adWriteRequest = (value: number): WSoftDjbRequest =>
  ({ op: 'write', field: 'ad', value, channel: LOAD_CELL_CHANNEL });

export const alarmReadRequest = (): WSoftDjbRequest =>
  ({ op: 'read', field: 'alarm', channel: LOAD_CELL_CHANNEL });
