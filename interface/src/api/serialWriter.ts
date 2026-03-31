import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import type { SerialWriterData, SerialWriterConfigUpdate, SerialWriterSendPayload } from '../types/serialWriter';

export const SERIAL_WRITER_ENDPOINT = 'serialWriter';

export function readSerialWriter(): AxiosPromise<SerialWriterData> {
  return AXIOS.get(SERIAL_WRITER_ENDPOINT);
}

/** Update serial writer config (baud rate, data bits, etc). */
export function updateSerialWriterConfig(data: SerialWriterConfigUpdate): AxiosPromise<SerialWriterData> {
  return AXIOS.post(SERIAL_WRITER_ENDPOINT, data);
}

/** Send a line to serial. Only pending_line is required; other fields are ignored. */
export function sendSerialWriterLine(payload: SerialWriterSendPayload): AxiosPromise<SerialWriterData> {
  return AXIOS.post(SERIAL_WRITER_ENDPOINT, payload);
}
