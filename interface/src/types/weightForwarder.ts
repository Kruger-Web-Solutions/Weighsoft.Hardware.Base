export enum ForwardProtocol {
  HTTP = 0,
  WS = 1,
  MQTT = 2,
  BLE = 3,
}

export enum OutputFormat {
  STANDARD = 0,
  LCD = 1,
  TFT = 2,
  SERIAL = 3,
}

export interface WeightForwarderData {
  protocol: ForwardProtocol;
  target_urls: string[];
  target_formats: OutputFormat[];
  target_url: string;
  ws_url: string;
  mqtt_topic: string;
  ble_service_uuid: string;
  ble_char_uuid: string;
  enabled: boolean;
  auth_username?: string;
  auth_password?: string;
  heartbeat_interval_sec: number;
  usb_echo_enabled: boolean;
  connected: boolean;
  last_error: string;
  last_forward_time: number;
}

export const HEARTBEAT_DEFAULT_SEC = 10;
export const HEARTBEAT_MIN_SEC = 1;
export const HEARTBEAT_MAX_SEC = 600;
