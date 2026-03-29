// Weighing Board -- TypeScript types

export interface WeighingData {
  // Live measurement (updated every loop cycle)
  weight: number;
  gross_weight: number;
  net_weight: number;
  tare_value: number;
  weight_mode: 'gross' | 'net';
  raw_value: number;
  stable: boolean;
  center_of_zero: boolean;
  overload: boolean;
  underload: boolean;
  adc_connected: boolean;
  // Configuration (persisted)
  unit: string;
  zero_offset: number;
  calibration_factor: number;
  max_capacity: number;
  min_division: number;
  decimal_places: number;
  zero_tracking_enabled: boolean;
  zero_tracking_range: number;
  power_on_zero_range: number;
  manual_zero_range: number;
  speed_mode: number;
  samples_to_average: number;
  // Seal / audit (PIN hash is NEVER included)
  seal_active: boolean;
  event_counter: number;
  last_calibration_time: string;
  pin_lockout_seconds: number;
}

export interface WeighingCommand {
  // Operational (not blocked by seal)
  tare?: boolean;
  preset_tare?: boolean;
  preset_tare_value?: number;
  clear_tare?: boolean;
  // Seal-gated operations
  zero?: boolean;
  calibrate?: boolean;
  known_weight?: number;
  // Seal management (factory PIN required for unseal)
  seal?: boolean;
  unseal?: boolean;
  pin?: string;
  // Config changes (blocked when sealed)
  unit?: string;
  max_capacity?: number;
  min_division?: number;
  decimal_places?: number;
  zero_tracking_enabled?: boolean;
  zero_tracking_range?: number;
  power_on_zero_range?: number;
  manual_zero_range?: number;
  speed_mode?: number;
  samples_to_average?: number;
}

export interface AuditEntry {
  seq: number;
  timestamp: string;
  event: string;
  parameter: string;
  old_value: string;
  new_value: string;
  event_counter: number;
}
