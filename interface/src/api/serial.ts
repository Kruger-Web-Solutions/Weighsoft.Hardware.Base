import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { SerialData, SerialConfigUpdate } from '../types/serial';

export const SERIAL_ENDPOINT = 'serial';

export function readSerialData(): AxiosPromise<SerialData> {
  return AXIOS.get<SerialData>(SERIAL_ENDPOINT).then((res) => {
    const d = res.data;
    const lastLine = d?.last_line ?? '';
    // #region agent log
    fetch('http://127.0.0.1:7717/ingest/0bfa9906-aaa9-4a5a-9ff1-54d549900d98', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Debug-Session-Id': '76d0d7' },
      body: JSON.stringify({
        sessionId: '76d0d7',
        hypothesisId: 'H-REST',
        location: 'serial.ts:readSerialData',
        message: 'rest_serial_snapshot',
        data: {
          lastLineLen: lastLine.length,
          timestamp: d?.timestamp ?? 0,
          weightLen: d?.weight != null ? String(d.weight).length : 0,
          baud: d?.baud_rate ?? 0,
          dbg_uart_rx_avail: d?.dbg_uart_rx_avail,
          dbg_suspended: d?.dbg_suspended,
          dbg_serial_started: d?.dbg_serial_started,
          dbg_line_buf_len: d?.dbg_line_buf_len,
        },
        timestamp: Date.now(),
      }),
    }).catch(() => {});
    // #endregion
    return res;
  });
}

/** POST only config fields; last_line/weight/timestamp are read-only and can cause 400 if resent. */
export function updateSerialData(data: SerialData | SerialConfigUpdate): AxiosPromise<SerialData> {
  const payload: SerialConfigUpdate = {
    baud_rate: data.baud_rate ?? 9600,
    data_bits: data.data_bits ?? 8,
    stop_bits: data.stop_bits ?? 1,
    parity: data.parity ?? 0,
    regex_pattern: data.regex_pattern ?? '',
  };
  return AXIOS.post(SERIAL_ENDPOINT, payload);
}

