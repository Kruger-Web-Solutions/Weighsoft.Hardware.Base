export interface SerialData {
  last_line: string;
  weight: string;
  timestamp: number;
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  regex_pattern: string;
}

/** Payload for POST /rest/serial: config only. Do not send last_line/weight/timestamp (read-only, can contain binary). */
export interface SerialConfigUpdate {
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  regex_pattern: string;
}
