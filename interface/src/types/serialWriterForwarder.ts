export enum ForwarderSourceProtocol {
  HTTP = 0,
  WS = 1,
}

/** Must match firmware JSON strings for `output_targets`. */
export enum ForwarderOutputTargets {
  UsbOnly = 'usb_only',
  Serial1Only = 'serial1_only',
  Both = 'both',
}

export interface SerialWriterForwarderData {
  // Config fields
  enabled: boolean;
  protocol: ForwarderSourceProtocol;
  source_url: string;
  json_field: string;
  poll_interval_ms: number;
  /** Where forwarder lines are written: USB CDC, Serial1 TX, or both. */
  output_targets?: ForwarderOutputTargets;
  auth_username?: string;
  auth_password?: string;

  // Runtime status (read-only from backend)
  connected: boolean;
  last_received_line: string;
  last_received_ms: number;
  received_count: number;
  last_error: string;
}
