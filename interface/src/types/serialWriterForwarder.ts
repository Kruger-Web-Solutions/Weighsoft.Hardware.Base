export enum ForwarderSourceProtocol {
  HTTP = 0,
  WS = 1,
}

export interface SerialWriterForwarderData {
  // Config fields
  enabled: boolean;
  protocol: ForwarderSourceProtocol;
  source_url: string;
  json_field: string;
  poll_interval_ms: number;
  auth_username?: string;
  auth_password?: string;

  // Runtime status (read-only from backend)
  connected: boolean;
  last_received_line: string;
  last_received_ms: number;
  received_count: number;
  last_error: string;
}
