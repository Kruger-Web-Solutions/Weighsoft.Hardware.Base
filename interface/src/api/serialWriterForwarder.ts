import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import type { SerialWriterForwarderData } from '../types/serialWriterForwarder';

export const SERIAL_WRITER_FORWARDER_ENDPOINT = 'serialWriterForwarder';

export function readSerialWriterForwarder(): AxiosPromise<SerialWriterForwarderData> {
  return AXIOS.get(SERIAL_WRITER_FORWARDER_ENDPOINT);
}

export function updateSerialWriterForwarder(data: SerialWriterForwarderData): AxiosPromise<SerialWriterForwarderData> {
  return AXIOS.post(SERIAL_WRITER_FORWARDER_ENDPOINT, data);
}
