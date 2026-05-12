import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { SerialData, SerialConfigUpdate } from '../types/serial';

export const SERIAL_ENDPOINT = 'serial';

export function readSerialData(): AxiosPromise<SerialData> {
  return AXIOS.get<SerialData>(SERIAL_ENDPOINT);
}

/** POST only config fields; last_line/weight/timestamp are read-only and can cause 400 if resent. */
export function updateSerialData(data: SerialData | SerialConfigUpdate): AxiosPromise<SerialData> {
  const payload: SerialConfigUpdate = {
    baud_rate: data.baud_rate ?? 9600,
    data_bits: data.data_bits ?? 8,
    stop_bits: data.stop_bits ?? 1,
    parity: data.parity ?? 0,
    regex_pattern: data.regex_pattern ?? '',
    publish_interval_ms: data.publish_interval_ms ?? 0,
  };
  return AXIOS.post(SERIAL_ENDPOINT, payload);
}

