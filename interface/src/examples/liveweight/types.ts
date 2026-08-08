export type LiveWeightSourceId = 0 | 1 | 2;
export type LiveWeightZoneId = 0 | 1 | 2 | 3;
export type DiActionId = 'none' | 'print' | 'next' | 'start' | 'stop';

export interface LiveWeightState {
  weight: string;
  last_line: string;
  timestamp: number;
  active_source: string;
  status_message: string;
  unit: string;
  zone: LiveWeightZoneId;
  zone_name: string;
  source: LiveWeightSourceId;
  source_name: string;
  baud_rate: number;
  regex_pattern: string;
  rs485_enabled: boolean;
  rs485_address: number;
  rs485_ready: boolean;
  range_enabled: boolean;
  range_low: number;
  range_high: number;
  relay_low: number;
  relay_ok: number;
  relay_high: number;
  plu: string;
  product: string;
  count: number;
  total: string;
  di1_action: DiActionId | string;
  di2_action: DiActionId | string;
  job_running: boolean;
  last_action: string;
  action_seq: number;
  printer_enabled: boolean;
  printer_ip: string;
  printer_port: number;
  /** Write-only: fire an action for testing (print / next / start / stop). */
  trigger_action?: DiActionId | string;
}

export const DEMO_LIVE_WEIGHT: LiveWeightState = {
  weight: '1.520',
  last_line: 'ST,GS,+  1.520 kg',
  timestamp: Date.now(),
  active_source: 'serial',
  status_message: 'In target range',
  unit: 'kg',
  zone: 2,
  zone_name: 'ok',
  source: 0,
  source_name: 'serial',
  baud_rate: 9600,
  regex_pattern: '([+-]?[0-9]+[\\.,]?[0-9]*)',
  rs485_enabled: false,
  rs485_address: 1,
  rs485_ready: false,
  range_enabled: true,
  range_low: 1,
  range_high: 2,
  relay_low: 1,
  relay_ok: 2,
  relay_high: 3,
  plu: '19',
  product: 'Screw M6',
  count: 3,
  total: '4.560',
  di1_action: 'print',
  di2_action: 'next',
  job_running: true,
  last_action: '',
  action_seq: 0,
  printer_enabled: false,
  printer_ip: '',
  printer_port: 9100
};

export const SOURCE_OPTIONS: {
  value: LiveWeightSourceId;
  label: string;
  help: string;
  disabled?: boolean;
}[] = [
  {
    value: 0,
    label: 'Serial (RS-232)',
    help: 'Scale cable into the MAX3232 / PROG header (UART0).'
  },
  {
    value: 1,
    label: 'WiFi / WebSocket',
    help: 'Another device posts weight to /rest/liveWeight or /ws/liveWeight.'
  },
  {
    value: 2,
    label: 'RS-485 (not available on this board)',
    help: 'No RS-485 transceiver on the ESP12F relay board. Not offered as a working source.',
    disabled: true
  }
];

export const RELAY_OPTIONS = [
  { value: 1, label: 'RY1' },
  { value: 2, label: 'RY2' },
  { value: 3, label: 'RY3' },
  { value: 4, label: 'RY4' }
];

export const DI_ACTION_OPTIONS: { value: DiActionId; label: string }[] = [
  { value: 'none', label: 'None' },
  { value: 'print', label: 'Print' },
  { value: 'next', label: 'Next' },
  { value: 'start', label: 'Start' },
  { value: 'stop', label: 'Stop' }
];
