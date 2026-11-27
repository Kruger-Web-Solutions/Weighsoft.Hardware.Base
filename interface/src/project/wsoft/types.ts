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
  /** An allowed junction box alarm is set (EEPROM factory flag): flagged, never blocking. */
  alarm_allowed: boolean;
  alarm_note: string | null;
  /** Raw DJB alarm state of the last good read, 8 hex digits. */
  alarm_word_hex: string | null;
  /** How often the alarm state changed since the board started. */
  alarm_changes: number;
  /** The alarm test switch is on (Settings). It only acts while Approval is OFF. */
  alarm_test?: boolean;
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
  /** rs485 = DJB junction box over Modbus; rs232 = a scale line read by the Serial reader;
   * loadcell = a load cell on an HX711 board, read and calibrated by this board. */
  source: WSoftSource;
  /** weigh = this board reads the scale and saves; writer = it shows a weigh board's weight and never saves. */
  role?: 'weigh' | 'writer';
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

export type WSoftSource = 'rs485' | 'rs232' | 'loadcell';

/** GET/POST /rest/wsoftLoadcell (admin): the load cell on an HX711 board and its calibration. */
export interface WSoftLoadcell {
  ok: boolean;
  /** A refused change: a code such as zero_first or too_small, and a sentence for the page. */
  error?: string;
  message?: string;
  /** The board's weight source is the load cell. */
  active: boolean;
  /** The HX711 answered in the last second. */
  present: boolean;
  /** The averaged raw count, or null when the HX711 is silent. It moves when the load cell is pressed. */
  raw: number | null;
  /** The count stored as zero; null before zero is set. */
  zero_raw: number | null;
  calibrated: boolean;
  zero_set: boolean;
  span_set: boolean;
  decimals: number;
  unit: 'kg' | 'g';
  /** Display step in digits: 1, 2, 5, 10, 20 or 50. */
  step: number;
  /** The filter: 0 off, 1 low, 2 medium, 3 high. */
  filter: number;
  /** Auto zero: 0 off, 1 to 3 = follows a creep within that many divisions of zero. */
  zero_track: number;
  /** Counts auto zero has moved the zero by since the last restart or zero. */
  zero_offset: number;
  /** Drift Guard: 0 off, 1 to 20 = the most divisions of slow drift it holds back. */
  drift_guard: number;
  /** Counts Drift Guard holds back right now. */
  drift_offset: number;
  /** Drift Guard is holding the weight shown. */
  drift_holding: boolean;
  /** Counts a second from the chip, measured (10 or 80 by its board); 0 when silent. */
  rate: number;
  /** How far the last raw counts are apart, in tenths of a division (10 = one division); null when not known. */
  noise_tenths: number | null;
  /** The test weight last used, without the decimal point; null before the first one. */
  test_weight_digits: number | null;
  /** The weight now, without the decimal point; null when there is none. */
  weight_digits: number | null;
  approval_on: boolean;
}

export type WSoftLoadcellRequest =
  | { action: 'zero' }
  | { action: 'span'; weight: string; unit: 'kg' | 'g' }
  /** The scale setup: unit, digits after the point, the division in the last digit (0.002 kg = 3, 2), the filter and auto zero. */
  | { action: 'format'; decimals: number; step: number; unit: 'kg' | 'g'; filter: number; zero_track: number; drift_guard: number };

// Mirrors src/examples/wsoft/WSoftSettingsService.h (WSoftSettings::read).
export interface WSoftPackSet {
  enabled: boolean;
  tolerance_g: number;
  targets_g: number[];
}

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
  /** Writer mode only: the relays follow the weigh board's weight. Off by default (relays stay off). */
  relay_writer_follow?: boolean;
  /** TEST SWITCH, off by default: save even when the junction box alarms (never with Approval ON). */
  alarm_test_save?: boolean;
  /** "ESP LED": the board's own light shows the Hi-Lo zone (orange / green / red). On by default. */
  esp_led?: boolean;
  /** Read-only: true on boards built with the on-board RGB LED (the S3 without a screen). */
  esp_led_available?: boolean;
  /** Hi-Lo light on the relay pins (yellow board LED1). Off by default. */
  relay_light?: boolean;
  /** Read-only: true on boards whose own light sits on the relay pins. */
  relay_light_available?: boolean;
  /** Read-only: true on boards built with relay outputs (the yellow board). */
  relay_available?: boolean;
  /** Read-only: false on boards without RS232 wiring (the Waveshare). */
  rs232_available?: boolean;
  /** Read-only: true on boards built with pins for a load cell on an HX711 board (the yellow board). */
  loadcell_available?: boolean;
  /** Read-only: base path with placeholders expanded, plus "/status". */
  mqtt_status_topic?: string;
  publish_interval_min_ms?: number;
  publish_interval_max_ms?: number;
  /** Pack-Size Auto-Save (off by default). Matches net weight against up to 8 targets when no Hi-Lo product is selected. */
  pack_set?: WSoftPackSet;
  /** Read-only: maximum target weights supported in pack_set.targets_g (8). */
  pack_set_max?: number;
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
  /** Every address of the same Hub this board may use, main one first; the rest are backups. */
  hub_urls?: string[];
  /** The address the board sends to now (moves to a backup when the Hub gives no answer). */
  hub_url_in_use?: string;
  key_id: string;
  device_id: string;
  /** HTTP status of the last heartbeat; 0 when none was sent since boot. */
  last_status: number;
  last_age_s: number;
  /** The board's clock came from the Hub because this network has no internet time. */
  clock_from_hub?: boolean;
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

// GET /rest/wsoftWifiUpdate (admin). The board never sends the password, only whether it has one.
export interface WSoftWifiUpdate {
  // false: this board holds one copy of the software only (the yellow board). Cable only.
  possible: boolean;
  on: boolean;
  password_set: boolean;
  approval_on: boolean;
  // The board is waiting for an update right now.
  listening: boolean;
  // Why it is not listening although switched on: ONE_COPY, APPROVAL_ON, PASSWORD_REQUIRED.
  blocked: string | null;
  blocked_message?: string;
  port: number;
  password_min: number;
  password_max: number;
  version: string;
}

// POST /rest/wsoftWifiUpdate (admin). A field that is not sent keeps its value.
export interface WSoftWifiUpdateRequest {
  on?: boolean;
  password?: string;
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
  /** The card has no format registers (DJB-W1X): decimals, e and unit are kept on the board, and card holds those. */
  format_on_board?: boolean;
  /** The weight comes from the RS485 weighing card (not a load cell or RS232). */
  rs485_card?: boolean;
  /** RS485 card: stable range / time and auto zero kept on this board (times in tenths of a second). */
  tuning?: WSoftRs485Tuning;
  /** RS485 card: auto zero runs on this board (the card has none of its own, the DJB-W1X). */
  zero_on_board?: boolean;
  /** RS485 card: the board's ZERO (the screen button), in the card's last digit. */
  zero?: { set: boolean; raw: number; auto: number; card_raw: number };
}

/** Kept on the board for an RS485 card. Ranges in steps of the last digit, times in tenths of a second. */
export interface WSoftRs485Tuning {
  stable_range: number;
  stable_time: number;
  zero_track_range: number;
  zero_track_time: number;
}

// A5. POST /rest/wsoftScaleSetup (admin).
export interface WSoftScaleSetupRequest {
  /** A field that is left out keeps its stored value. */
  board: Partial<WSoftScaleBoard>;
  reason: string;
  /** Only for a card with no format registers: decimals, e in card steps and unit, kept on the board. */
  format?: WSoftScaleCard;
  /** RS485 card tuning kept on this board; a key left out keeps its value. */
  tuning?: Partial<WSoftRs485Tuning>;
}

// POST /rest/djb (admin), src/examples/wsoft/WSoftStatusService.cpp handleDjb.
export type WSoftCardField =
  'division' | 'decimal' | 'unit' | 'zero_track_limit' | 'zero_track_time' | 'stability_range' | 'stability_time'
  | 'filter1' | 'filter2';

// 'ad' is the calibration register (DJB Communication Protocol V2.0, LC1 AD 41003): write 0 = zero
// calibration, write CW > 0 = load calibration with the test weight on. 'alarm' is read-only.
export type WSoftDjbRequest =
  | { op: 'read'; field: WSoftCardField | 'alarm'; channel?: number }
  | { op: 'write'; field: WSoftCardField | 'ad'; value: number; channel?: number };

export interface WSoftDjbAlarmBits {
  parameter_changed: boolean;
  software_match: boolean;
  adc_failure: boolean;
  over_range: boolean;
  eeprom: boolean;
}

export interface WSoftDjbReply {
  ok: boolean;
  sent: boolean;
  /** Lower-case code, e.g. not_linked, approval_on, range, busy. */
  error?: string;
  value?: number;
  /** The Modbus frames, for a bench check. */
  request_hex?: string;
  reply_hex?: string;
  /** Only on an alarm read. */
  alarm_bits?: WSoftDjbAlarmBits;
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
  hilo: { enabled: boolean; light?: boolean; relay_low: number; relay_ok: number; relay_high: number; zone: string };
  relays: WSoftRelayBoard;
}

/** One product on GET /rest/wsoftProducts. Limits in whole grams. */
export interface WSoftProduct {
  code: string;
  name: string;
  low_g: number;
  target_g: number;
  high_g: number;
  /** hub = pulled from the Hub (read-only here); board = made on this board. */
  from: 'hub' | 'board';
  /** A board product the Hub now has the same code for: the Hub one is used. */
  hidden: boolean;
}

export interface WSoftProducts {
  /** Code in use, "" = no product. */
  current: string;
  board_max: number;
  products: WSoftProduct[];
}

export type WSoftProductInput = Pick<WSoftProduct, 'code' | 'name' | 'low_g' | 'target_g' | 'high_g'>;

export type WSoftProductAction =
  | { action: 'put'; old_code: string; product: WSoftProductInput }
  | { action: 'delete'; code: string }
  | { action: 'use'; code: string };
