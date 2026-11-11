export interface Features {
  project: boolean;
  security: boolean;
  mqtt: boolean;
  ntp: boolean;
  ota: boolean;
  upload_firmware: boolean;
  ble: boolean;
  /** A W-Soft weigh screen build. Older firmware does not send these three. */
  wsoft?: boolean;
  /** W-Soft board with Hi-Lo relay outputs (the LED example is not created there). */
  wsoft_relay?: boolean;
  /** W-Soft board wired for an RS232 scale: the Serial page sets its speed and pattern. */
  wsoft_rs232?: boolean;
}
