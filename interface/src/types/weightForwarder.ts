export enum ForwardProtocol {
  HTTP = 0,
  WS = 1,
  MQTT = 2,
  BLE = 3,
}

export interface WeightForwarderData {
  protocol: ForwardProtocol;
  target_url: string;
  ws_url: string;
  mqtt_topic: string;
  ble_service_uuid: string;
  ble_char_uuid: string;
  enabled: boolean;
  display_mode: boolean;
  auth_username?: string;
  auth_password?: string;
  /** MAC-derived device unique id; read-only in UI; included in forwarded payloads */
  device_id: string;
  connected: boolean;
  last_error: string;
  last_forward_time: number;
}
