export type UartMode = 'reader' | 'writer' | '';

export interface UartModeData {
  mode: UartMode;  // '' = NEW (not yet configured)
}
