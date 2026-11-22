export interface LedExampleState {
  led_on: boolean;
  red: number;      // 0-255
  green: number;    // 0-255
  blue: number;     // 0-255
  has_rgb: boolean; // True if hardware supports RGB (ESP32-S3)
}
