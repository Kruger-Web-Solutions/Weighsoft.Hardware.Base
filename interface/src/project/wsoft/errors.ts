import { AxiosError } from 'axios';

import { extractErrorMessage } from '../../utils';

import { WSoftBoardError } from './types';

// Board codes (switches, PIN, scale setup and /rest/djb) as plain sentences.
const PLAIN: Record<string, string> = {
  ALREADY: 'Nothing changed: the switch is already set that way.',
  SETUP_INCOMPLETE: 'Fill in the Scale setup first.',
  REASON_REQUIRED: 'Type a short reason.',
  REASON_TOO_LONG: 'Keep the reason to 80 characters or less.',
  BUSY: 'The board is busy. Try again in a moment.',
  UNCONFIRMED: 'The board took the change but did not confirm it in time. Check the switch again in a moment.',
  WHO_REQUIRED: 'Sign in again: the board could not tell who you are.',
  NO_CARD: 'No weighing card: the scale is on RS232.',
  PIN_FORMAT: 'The PIN must be 4 to 6 digits.',
  PIN_WRONG: 'Wrong PIN.',
  PIN_LOCKED: 'Too many wrong PINs. The PIN pad is locked for a few minutes.',
  PIN_NOT_SET: 'No Approval PIN yet. An admin sets it on the Settings tab.',
  SETUP_LOCKED: 'Switch Approval off to change the setup.',
  SETUP_ORDER: 'Check Max and Min: Max must be more than e, Min at least e, and Min less than Max.',
  SETUP_NUMBER: 'Check the kg values: use whole steps of e, with up to 5 decimals.',
  SETUP_TEXT: 'The SA number must be 1 to 32 characters.',
  SETUP_RANGE:
    'A value is out of range: stable samples 3 to 20, blank counts 0 to 100, ' +
    'most tare up to Max, preset tare up to most tare.',
  COMMAND_EXPIRED: 'The command waited too long and was dropped.',
  BAD_CODE: 'Code: 1 to 32 letters, numbers, - or _. No spaces.',
  BAD_NAME: 'Name: 1 to 24 plain letters, numbers or signs.',
  BAD_LIMITS: 'Low must be below High, and Target between them.',
  HUB_CODE: 'That code belongs to the Hub. Pick another code.',
  DUPLICATE: 'This board already has a product with that code.',
  FULL: 'This board holds 20 products. Delete one first.',
  NOT_FOUND: 'That product is not on this board any more.',
  // /rest/djb codes are lower case.
  not_linked: 'The weighing card is not linked. Check the RS485 cable.',
  approval_on: 'Switch Approval off to change the setup.',
  range: 'That value is out of range for the weighing card.',
  busy: 'The board is busy. Try again in a moment.',
  timeout: 'The weighing card did not answer in time.',
  bad_value: 'Type a whole number.',
  read_only: 'This value can only be read.',
  json: 'The board could not read the request.'
};

export const plainError = (code?: string | null) => (code ? PLAIN[code] ?? code : undefined);

/** One plain sentence for any failed board call. */
export const boardErrorText = (error: unknown, fallback: string) => {
  const axiosError = error as AxiosError<WSoftBoardError>;
  const status = axiosError?.response?.status;
  if (status === 401 || status === 403) return 'Only an admin can change this. Sign in as an admin.';
  const data = axiosError?.response?.data;
  if (data?.error && PLAIN[data.error]) return PLAIN[data.error];
  if (data?.message) return data.message;
  if (data?.error) return data.error;
  return extractErrorMessage(axiosError as AxiosError<{ message?: string }>, fallback);
};
