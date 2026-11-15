import { WSoftWifiUpdate } from './types';

// The same password rule as the board (lib/wsoft_core/src/wsoft_wifi_update.cpp). The board
// checks again; this only saves a round trip and gives the hint while typing.
export const passwordProblem = (password: string, min: number, max: number): string | undefined => {
  if (password.length === 0) return undefined;
  if (password.length < min) return `At least ${min} characters`;
  if (password.length > max) return `At most ${max} characters`;
  if (!/^[\x21-\x7e]+$/.test(password)) return 'Letters, digits and signs only. No spaces.';
  return undefined;
};

export interface WifiUpdateChip {
  label: string;
  color: 'success' | 'warning' | 'default' | 'error';
}

/** One chip that says where the switch stands, in plain words. */
export const wifiUpdateChip = (info: WSoftWifiUpdate): WifiUpdateChip => {
  if (!info.possible) return { label: 'Cable only', color: 'default' };
  if (!info.on) return { label: 'OFF', color: 'default' };
  if (info.blocked === 'APPROVAL_ON') return { label: 'ON, locked: Approval is ON', color: 'warning' };
  if (info.blocked) return { label: 'ON, not ready', color: 'warning' };
  if (info.listening) return { label: 'ON, ready for an update', color: 'success' };
  return { label: 'ON, waiting for Wi-Fi', color: 'warning' };
};
