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
}

// Mirrors src/examples/wsoft/WSoftSettingsService.h (WSoftSettings::read).
export interface WSoftSettings {
  publish_interval_ms: number;
  mqtt_enabled: boolean;
  mqtt_base_path: string;
  mqtt_name: string;
  mqtt_unique_id: string;
  /** Read-only: base path with placeholders expanded, plus "/status". */
  mqtt_status_topic?: string;
  publish_interval_min_ms?: number;
  publish_interval_max_ms?: number;
}

// Mirrors src/examples/wsoft/WSoftHubService.cpp (GET /rest/wsoftHub). Admin only.
export interface WSoftHubStatus {
  configured: boolean;
  /** Same words as the box's screen: "HUB ONLINE", "HUB OFFLINE", "HUB CLOCK", ... */
  state: string;
  online: boolean;
  /** The Hub address the box is sending to now. */
  hub_url?: string;
  /** Every LAN address of the Hub the box can try, in order (one for an old code). */
  hub_urls?: string[];
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
