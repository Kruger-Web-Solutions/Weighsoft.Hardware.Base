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
  // Saving a weighing (src/examples/wsoft/WSoftStatusService.cpp, WSoftStatus::read).
  /** A SAVE press now would be accepted. */
  save_can: boolean;
  /** "ok", "moving", "alarm", "no_signal", "stale", "empty", "saved", "unit", "no_clock", "queue_full". */
  save_reason: string;
  save_auto: boolean;
  save_auto_s: number;
  /** 0 when nothing was saved since start-up. */
  save_last_seq: number;
  save_last_text: string | null;
  /** Saved weighings waiting for the Hub. */
  save_queue: number;
  /** Last HTTP status from the Hub for a weighing; 0 before the first. */
  save_hub_status: number;
}

// POST /rest/wsoftSave answer (200 saved, 409 refused with reason, 503 reason "busy").
export interface WSoftSaveResult {
  ok: boolean;
  seq?: number;
  weight_text?: string;
  reason?: string;
}

// GET /rest/wsoftSave
export interface WSoftSavedWeighing {
  seq: number;
  weight_text: string;
  /** UTC, e.g. "2026-10-31T12:00:00Z". */
  at: string | null;
  auto_saved: boolean;
  trade: boolean;
  /** The Hub has it. */
  delivered: boolean;
}

export interface WSoftSavedList {
  queue: number;
  hub_status: number;
  /** Newest first, at most 10. */
  recent: WSoftSavedWeighing[];
}

// Mirrors src/examples/wsoft/WSoftSettingsService.h (WSoftSettings::read).
export interface WSoftSettings {
  publish_interval_ms: number;
  mqtt_enabled: boolean;
  mqtt_base_path: string;
  mqtt_name: string;
  mqtt_unique_id: string;
  /** Save a load once it has been still for auto_save_stable_s seconds. */
  auto_save: boolean;
  /** 1 to 30 seconds. */
  auto_save_stable_s: number;
  /** Read-only: base path with placeholders expanded, plus "/status". */
  mqtt_status_topic?: string;
  publish_interval_min_ms?: number;
  publish_interval_max_ms?: number;
  auto_save_stable_min_s?: number;
  auto_save_stable_max_s?: number;
}

// Mirrors src/examples/wsoft/WSoftHubService.cpp (GET /rest/wsoftHub). Admin only.
export interface WSoftHubStatus {
  configured: boolean;
  /** Same words as the box's screen: "HUB ONLINE", "HUB OFFLINE", "HUB CLOCK", ... */
  state: string;
  online: boolean;
  hub_url?: string;
  key_id?: string;
  /** Set when the Hub address can't work from this box (Tailscale or localhost). */
  warning?: string;
  /** Last HTTP status from the Hub; 0 before the first try. */
  last_status: number;
  clock_valid: boolean;
  clock_from_hub: boolean;
}

// POST /rest/wsoftHub answer.
export interface WSoftHubPairResult {
  ok: boolean;
  message: string;
  hub_url?: string;
  warning?: string;
  restarting?: boolean;
}

// POST /rest/djb (src/examples/wsoft/WSoftStatusService.cpp handleDjb). Admin only.
export interface WSoftDjbRequest {
  op: 'read' | 'write';
  field: string;
  channel?: number;
  value?: number;
}

export interface WSoftDjbResult {
  ok: boolean;
  sent: boolean;
  /** e.g. "approval_on", "not_linked", "busy", "ad_range". */
  error?: string;
  value?: number;
  exception?: number;
  /** The Modbus frames, "01 10 03 EA ..." hex. */
  request_hex?: string;
  reply_hex?: string;
  /** Alarm reads only. */
  alarm_bits?: {
    parameter_changed: boolean;
    software_match: boolean;
    adc_failure: boolean;
    over_range: boolean;
    eeprom: boolean;
  };
}
