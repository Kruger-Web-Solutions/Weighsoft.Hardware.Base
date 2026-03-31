export type LineTerminator = 'LF' | 'CRLF' | 'CR' | 'NONE';

export interface SerialWriterData {
  // Config fields
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  line_terminator: LineTerminator;

  // Runtime status (read-only from backend)
  pending_line: string;
  last_sent_line: string;
  last_sent_ms: number;
  sent_count: number;
}

/** POST payload — config changes only. pending_line is the write trigger. */
export interface SerialWriterConfigUpdate {
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  line_terminator: LineTerminator;
}

/** POST payload — send a line to serial. */
export interface SerialWriterSendPayload {
  pending_line: string;
}
