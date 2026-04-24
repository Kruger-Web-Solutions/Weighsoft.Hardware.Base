export interface SerialData {
  last_line: string;
  weight: string;
  timestamp: number;
  /** STA MAC as 12 hex chars; matches MQTT #{unique_id} segment. */
  device_id: string;
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  regex_pattern: string;
  /** Firmware-only: UART snapshot from last SerialService::loop (not persisted). */
  dbg_uart_rx_avail?: number;
  dbg_suspended?: number;
  dbg_serial_started?: number;
  dbg_line_buf_len?: number;
}

/** Payload for POST /rest/serial: config only. Do not send last_line/weight/timestamp (read-only, can contain binary). */
export interface SerialConfigUpdate {
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  regex_pattern: string;
}
