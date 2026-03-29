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
  connected: boolean;
  last_error: string;
  last_forward_time: number;
}
