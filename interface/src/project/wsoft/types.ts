// Mirrors lib/wsoft_core/src/wsoft_view.cpp (wsoft_view_write) field for field.
export type WSoftLink = 'starting' | 'no_signal' | 'no_reply' | 'alarm' | 'live';

export interface WSoftStatus {
  ready: boolean;
  display_ready: boolean;
  link: WSoftLink;
  linked: boolean;
  /** Whole grams. null when the DJB is not linked or the last poll failed. */
  weight_g: number | null;
  weight_text: string | null;
  unit: string;
  decimals: number;
  stable: boolean;
  alarm: boolean;
  /** Read-only. Approval is switched with a USB cable only. */
  approval_on: boolean;
  for_trade: boolean;
  trade_check: string;
  audit_counter: number;
  last_trade_seq: number;
  trade_queue: number;
  hub: string;
  hub_online: boolean;
  polls: number;
  /** rs485 = DJB junction box over Modbus; rs232 = a scale line read by the Serial reader. */
  source: WSoftSource;
}

export type WSoftSource = 'rs485' | 'rs232';

// Mirrors src/examples/wsoft/WSoftSettingsService.h (WSoftSettings::read).
export interface WSoftSettings {
  publish_interval_ms: number;
  mqtt_enabled: boolean;
  mqtt_base_path: string;
  mqtt_name: string;
  mqtt_unique_id: string;
  /** Where the weight comes from. Saving a new source restarts the board. */
  source: WSoftSource;
  /** Save a weighing for the Hub by itself when a new load settles. Off: only the Save button. */
  auto_save: boolean;
  /** Read-only: false on boards without RS232 wiring (the Waveshare). */
  rs232_available?: boolean;
  /** Read-only: base path with placeholders expanded, plus "/status". */
  mqtt_status_topic?: string;
  publish_interval_min_ms?: number;
  publish_interval_max_ms?: number;
}

// Mirrors src/examples/wsoft/WSoftStatusService.cpp (sendHub). Admin only.
// The pairing key is write-only: the board never sends it back.
export interface WSoftHub {
  ok: boolean;
  error?: string;
  /** A valid pairing key is saved on the board. */
  paired: boolean;
  /** HUB SETUP / HUB NO WIFI / HUB CLOCK / HUB ONLINE / HUB REJECTED / HUB OFFLINE */
  state: string;
  online: boolean;
  hub_url: string;
  key_id: string;
  device_id: string;
  /** HTTP status of the last heartbeat; 0 when none was sent since boot. */
  last_status: number;
  last_age_s: number;
  /** True right after connect/disconnect: the board is restarting. */
  restarting: boolean;
}

export type WSoftHubAction = { action: 'connect'; code: string } | { action: 'disconnect' };

// Mirrors src/examples/wsoft/WSoftStatusService.cpp (sendSave).
export interface WSoftWeighing {
  seq: number;
  weight: string;
  unit: string;
  /** Unix seconds. */
  at: number;
}

export interface WSoftSave {
  auto_save: boolean;
  /** Saved on the board, not yet accepted by the Hub. */
  pending: number;
  last: WSoftWeighing | null;
  /** Only on POST: whether this tap saved, and why not. */
  ok?: boolean;
  result?: string;
  saved?: WSoftWeighing;
}
