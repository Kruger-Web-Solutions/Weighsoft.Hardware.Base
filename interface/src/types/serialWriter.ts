export type LineEnding = 0 | 1 | 2 | 3;  // None, CR, LF, CRLF

export type TxSource = 0 | 1 | 2 | 3 | 4 | 5;
// NONE, MANUAL, REST, WS, MQTT, READER

export interface SerialWriterData {
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  line_ending: LineEnding;
  output_port: 0 | 1;  // 0=Serial1 (GPIO pins), 1=USB Serial
  /** Throttle interval (ms) for outbound serial writes. 0 = transmit every received line. */
  transmit_interval_ms: number;
  friendly_name: string;
  mqtt_subscribe_topic: string;
  mqtt_status_topic: string;
  mqtt_enabled: boolean;
  device_id: string;

  last_sent: string;
  last_sent_at: number;
  last_sent_source: TxSource;
  bytes_sent_total: number;
  suspended: number;
  serial_started: number;
}

export interface SerialWriterForwarderData {
  source_url: string;
  connection_method: 0 | 1;  // 0=WS, 1=HTTP
  auto_reconnect: boolean;
  enabled: boolean;

  connected: boolean;
  last_received_at: number;
  last_received: string;
  reconnect_attempts: number;
  last_error: string;
}
