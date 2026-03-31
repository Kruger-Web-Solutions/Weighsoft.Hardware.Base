export interface GrowboxState {
  // Relay outputs
  lights_on: boolean;
  light_a_on: boolean;
  light_b_on: boolean;
  fan_in_on: boolean;
  fan_out_on: boolean;

  // Operating modes: "manual" | "schedule" | "auto"
  lights_mode: string;
  fan_in_mode: string;
  fan_out_mode: string;

  // Sensors (Phase 2)
  inside_temperature: number;
  inside_humidity: number;
  outside_temperature: number;
  outside_humidity: number;
  moisture_raw: number;
  moisture_percent: number;
  moisture_calibrated: boolean;

  // Alarms (Phase 3)
  moisture_alarm_active: boolean;
  moisture_alarm_acknowledged: boolean;

  // Sensor health (Phase 2)
  inside_sensor_fault: boolean;
  outside_sensor_fault: boolean;
  moisture_sensor_fault: boolean;

  // Reason strings (Phase 3)
  lights_reason: string;
  fan_in_reason: string;
  fan_out_reason: string;
  moisture_alarm_reason: string;

  // Diagnostics
  schedule_active: boolean;
  manual_override_active: boolean;
  uptime_ms: number;
  boot_count: number;
  ntp_synced: boolean;
  wifi_connected: boolean;
}

export interface GrowboxAutomationSettings {
  light_schedule_enabled: boolean;
  light_on_time: string;
  light_off_time: string;
  fan_control_enabled: boolean;
  fan_temperature_threshold: number;
  fan_hysteresis: number;
  moisture_alarm_enabled: boolean;
  moisture_alarm_threshold: number;
  moisture_alarm_dwell_ms: number;
}

// Partial update type — only writable fields exposed to external channels
export interface GrowboxStateUpdate {
  lights_on?: boolean;
  light_a_on?: boolean;
  light_b_on?: boolean;
  fan_in_on?: boolean;
  fan_out_on?: boolean;
  moisture_alarm_acknowledged?: boolean;
}

export interface GrowboxSettings {
  light_relay_a_pin: number;
  light_relay_b_pin: number;
  fan_in_pin: number;
  fan_out_pin: number;
  i2c_sda_pin: number;
  i2c_scl_pin: number;
  dht_pin: number;
  moisture_adc_pin: number;
  moisture_dry_value: number;
  moisture_wet_value: number;
  relay_active_high: boolean;
  poll_interval_ms: number;
  inside_sensor_enabled: boolean;
  outside_sensor_enabled: boolean;
  moisture_sensor_enabled: boolean;
}
