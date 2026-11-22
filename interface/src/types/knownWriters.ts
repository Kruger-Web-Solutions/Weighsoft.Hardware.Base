export interface KnownWriter {
  id: string;
  friendly_name: string;
  ip: string;
  // Elapsed milliseconds since each event, computed by the device at read time.
  // 0 = "never happened" (no event recorded yet this boot).
  first_seen_ms_ago: number;
  last_seen_ms_ago: number;
  last_message_ms_ago: number;
  last_message: string;
  online: boolean;
}

export interface KnownWritersData {
  writers: KnownWriter[];
}
