export interface RelayBoardState {
  relay1: boolean;
  relay2: boolean;
  relay3: boolean;
  relay4: boolean;
  buzzer: boolean;
  di1: boolean;
  di2: boolean;
}

export interface RelayBoardStatus {
  board: string;
  mcu: string;
  platform: string;
  chip_id: string;
  cpu_freq_mhz: number;
  free_heap: number;
  heap_fragmentation: number;
  flash_chip_size: number;
  flash_chip_speed?: number;
  sketch_size: number;
  free_sketch_space: number;
  sdk_version: string;
  core_version?: string;
  uptime_ms?: number;
  reset_reason?: string;
  vcc_mv?: number;
  wifi_ssid?: string;
  wifi_rssi?: number;
  ip?: string;
  mac?: string;
  has_temp_sensor: boolean;
  has_buzzer: boolean;
  power_led: string;
  relay_active: string;
  pins: {
    ry1: number;
    ry2: number;
    ry3: number;
    ry4: number;
    di1?: number;
    di2?: number;
    buzzer?: number;
  };
  gpio_legend: Record<string, string>;
  relays: RelayBoardState;
}

export const DEMO_RELAY_STATE: RelayBoardState = {
  relay1: false,
  relay2: false,
  relay3: false,
  relay4: false,
  buzzer: false,
  di1: false,
  di2: false
};

export const DEMO_BOARD_STATUS: RelayBoardStatus = {
  board: 'RelayBoardEspBuildIn',
  mcu: 'ESP-12F',
  platform: 'esp8266',
  chip_id: 'demo',
  cpu_freq_mhz: 160,
  free_heap: 38000,
  heap_fragmentation: 12,
  flash_chip_size: 4194304,
  flash_chip_speed: 40000000,
  sketch_size: 860000,
  free_sketch_space: 1500000,
  sdk_version: 'demo',
  core_version: 'demo',
  uptime_ms: 754000,
  reset_reason: 'Power On',
  vcc_mv: 3288,
  wifi_ssid: 'demo-network',
  wifi_rssi: -58,
  ip: '192.168.4.1',
  mac: 'DE:MO:DE:MO:DE:MO',
  has_temp_sensor: false,
  has_buzzer: true,
  power_led: 'hardwired',
  relay_active: 'low',
  pins: { ry1: 16, ry2: 14, ry3: 12, ry4: 13, di1: 4, di2: 5, buzzer: 15 },
  gpio_legend: {
    '16': 'DO RY1',
    '14': 'DO RY2',
    '12': 'DO RY3',
    '13': 'DO RY4',
    '4': 'DI 1 (pullup)',
    '5': 'DI 2 (pullup)',
    '15': 'DO buzzer',
    '0': 'BOOT strap / flash',
    '2': 'BOOT strap',
    '1': 'TX0 UART',
    '3': 'RX0 UART'
  },
  relays: DEMO_RELAY_STATE
};
