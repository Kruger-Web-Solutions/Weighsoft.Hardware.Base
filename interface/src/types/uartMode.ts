export type UartMode = 'reader' | 'writer' | 'diagnostics' | '';

export interface UartModeData {
  mode: UartMode;  // '' = NEW (not yet configured)
}
