export type LiveWeightSourceId = 0 | 1 | 2;

export interface LiveWeightState {
  weight: string;
  last_line: string;
  timestamp: number;
  active_source: string;
  status_message: string;
  source: LiveWeightSourceId;
  source_name: string;
  baud_rate: number;
  regex_pattern: string;
  rs485_enabled: boolean;
  rs485_address: number;
  rs485_ready: boolean;
}

export const DEMO_LIVE_WEIGHT: LiveWeightState = {
  weight: '12.34',
  last_line: 'ST,GS,+  12.34 kg',
  timestamp: Date.now(),
  active_source: 'wifi',
  status_message: 'Demo weight (no board connected)',
  source: 1,
  source_name: 'wifi',
  baud_rate: 9600,
  regex_pattern: '([+-]?[0-9]+[\\.,]?[0-9]*)',
  rs485_enabled: false,
  rs485_address: 1,
  rs485_ready: false
};

export const SOURCE_OPTIONS: { value: LiveWeightSourceId; label: string; help: string }[] = [
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
    label: 'RS-485 (coming soon)',
    help: 'Needs an RS-485 transceiver module. Stubbed for now.'
  }
];
