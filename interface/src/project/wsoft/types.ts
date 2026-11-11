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
  /** Read-only here. Switch it with POST /rest/wsoftSwitches (WSoftSwitches). */
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
  /** What stops a save right now (wsoft_save_notice). All null when nothing does. */
  notice_code: string | null;
  /** What to do, in capitals, e.g. TAKE THE LOAD OFF. */
  notice_title: string | null;
  /** Why, e.g. The scale must show 0 kg first. */
  notice_detail: string | null;
  /** stop = red, nothing saves until the person acts; wait = yellow; info = all is well. */
  notice_level: WSoftNoticeLevel | null;
}

export type WSoftNoticeLevel = 'stop' | 'wait' | 'info';

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
  /** Hi-Lo relay output: off by default. */
  relay_enabled: boolean;
  /** The relay (1 to 4, 0 = none) for each Hi-Lo zone. No relay twice. */
  relay_low: number;
  relay_ok: number;
  relay_high: number;
  /** Read-only: true on boards built with relay outputs (the yellow board). */
  relay_available?: boolean;
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
  /** Saved on the board, not yet accepted by the Hub. */
  pending: number;
  last: WSoftWeighing | null;
  /** Only on POST: whether this tap saved, and why not. */
  ok?: boolean;
  result?: string;
  saved?: WSoftWeighing;
}

// Contract A (docs/superpowers/plans/2026-11-07-wsoft-switches-scale-setup.md).
// Board errors: 409 (or 400 for a bad shape) with { error: CODE, message }.
export interface WSoftBoardError {
  error?: string;
  message?: string;
}

// A1. GET /rest/wsoftSwitches (logged-in users).
export interface WSoftApproval {
  on: boolean;
  audit_counter: number;
  trade_check: string;
  can_switch_on: boolean;
  /** null, or a code such as SETUP_INCOMPLETE. */
  blocked: string | null;
}

export interface WSoftSwitches {
  source: WSoftSource;
  approval: WSoftApproval;
  auto_save: boolean;
  /** An Approval PIN for the screen is set. The PIN itself is never sent. */
  pin_set: boolean;
  /** Seconds the screen PIN pad stays locked after wrong tries. 0 = not locked. */
  pin_locked_s: number;
  /** Last 8 approval log lines, oldest first. */
  log: string[];
}

export type WSoftSwitchName = 'approval' | 'auto_save';

// A2. POST /rest/wsoftSwitches (admin). Reason: 1 to 80 characters for approval, optional for auto_save.
export interface WSoftSwitchRequest {
  switch: WSoftSwitchName;
  on: boolean;
  reason: string;
}

// A3. POST /rest/wsoftPin (admin).
export interface WSoftPinRequest {
  pin: string;
}

export interface WSoftPinReply {
  pin_set: boolean;
}

// A4. GET /rest/wsoftScaleSetup (logged-in users). kg values are decimal text, "" = not set.
export interface WSoftScaleBoard {
  sa_number: string;
  max_kg: string;
  min_kg: string;
  /** Counts of e. null = not set. */
  blank_below_e: number | null;
  blank_above_e: number;
  stable_samples: number;
  tare_enabled: boolean;
  tare_max_kg: string;
  preset_tare_kg: string;
}

export interface WSoftScaleCard {
  division: number;
  decimal: number;
  unit: number;
}

export type WSoftSetupLock = 'APPROVAL_ON' | 'RS232';

export interface WSoftScaleSetup {
  source: WSoftSource;
  editable: boolean;
  locked: WSoftSetupLock | null;
  complete: boolean;
  missing: string[];
  setup_audit: number;
  board: WSoftScaleBoard;
  /** Last values read from the weighing card; null when not read yet. */
  card: WSoftScaleCard | null;
}

// A5. POST /rest/wsoftScaleSetup (admin).
export interface WSoftScaleSetupRequest {
  board: WSoftScaleBoard;
  reason: string;
}

// POST /rest/djb (admin), src/examples/wsoft/WSoftStatusService.cpp handleDjb.
export type WSoftCardField =
  'division' | 'decimal' | 'unit' | 'zero_track_limit' | 'zero_track_time' | 'stability_range' | 'stability_time';

export type WSoftDjbRequest =
  { op: 'read'; field: WSoftCardField } | { op: 'write'; field: WSoftCardField; value: number };

export interface WSoftDjbReply {
  ok: boolean;
  sent: boolean;
  /** Lower-case code, e.g. not_linked, approval_on, range, busy. */
  error?: string;
  value?: number;
}

// Mirrors src/examples/relay/RelayBoardService.h (RelayBoardState).
export interface WSoftRelayBoard {
  relay1: boolean;
  relay2: boolean;
  relay3: boolean;
  relay4: boolean;
}

// GET /rest/relayBoardStatus: pins (-1 = not fitted), the Hi-Lo zone and the relays now.
export interface WSoftRelayStatus {
  relay_active: 'low' | 'high';
  pins: { ry1: number; ry2: number; ry3: number; ry4: number };
  hilo: { enabled: boolean; relay_low: number; relay_ok: number; relay_high: number; zone: string };
  relays: WSoftRelayBoard;
}
