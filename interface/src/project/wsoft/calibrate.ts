// Calibrate tab rules (WSoftCalibrate.tsx), kept apart so host tests can pin them.
//
// The DJB junction box calibrates with its AD register (DJB-W/M1-16 Communication Protocol V2.0,
// LC1 AD Value 41003, command 10): write 0 = zero calibration with the scale empty; write the
// calibrating weight CW (> 0) = load calibration with that test weight on. CW is the weight as the
// box shows it, without the decimal point (2.000 kg at 3 decimals = 2000), as on the 2 Nov 2026
// Waveshare branch. The board refuses the write while Approval is ON, and counts every write
// that goes through on the audit counter.

import { WSoftStatus } from './types';

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
