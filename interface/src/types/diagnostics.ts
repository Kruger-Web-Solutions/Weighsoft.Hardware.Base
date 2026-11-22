export interface LoopbackState {
  enabled: boolean;
  status: 'idle' | 'running' | 'pass' | 'fail';
  tx_count: number;
  rx_count: number;
  error_count: number;
  success_rate: number;
  last_test: string;
  last_received: string;
  uptime_seconds: number;
}

export interface BaudScanState {
  enabled: boolean;
  status: 'idle' | 'scanning' | 'found' | 'not_found';
  detected_baud: number;
  current_index: number;
  current_baud?: number;
  test_packets: number;
}

export interface SignalQualityState {
  enabled: boolean;
  status: 'idle' | 'running' | 'complete';
  quality_percent: number;
  total_packets: number;
  sent_packets: number;
  received_packets: number;
  avg_latency_ms: number;
  jitter_ms: number;
  error_count: number;
  progress: number;
}

export interface DiagnosticsData {
  loopback: LoopbackState;
  baud_scan: BaudScanState;
  signal_quality: SignalQualityState;
}
