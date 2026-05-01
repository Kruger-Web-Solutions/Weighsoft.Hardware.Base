import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { SerialWriterData, SerialWriterForwarderData } from '../types/serialWriter';

export const SERIALW_REST_PATH = '/rest/serialWriter';
export const SERIALW_SEND_PATH = '/rest/serialWriter/send';
export const SERIALW_FWD_PATH  = '/rest/serialWriterSource';

export const readSerialWriter = (): AxiosPromise<SerialWriterData> =>
  AXIOS.get(SERIALW_REST_PATH);

export const updateSerialWriter = (data: Partial<SerialWriterData>): AxiosPromise<SerialWriterData> =>
  AXIOS.post(SERIALW_REST_PATH, data);

export const sendWriterMessage = (data: string): AxiosPromise<{ sent: boolean }> =>
  AXIOS.post(SERIALW_SEND_PATH, { data });

export const readWriterSource = (): AxiosPromise<SerialWriterForwarderData> =>
  AXIOS.get(SERIALW_FWD_PATH);

export const updateWriterSource = (data: Partial<SerialWriterForwarderData>): AxiosPromise<SerialWriterForwarderData> =>
  AXIOS.post(SERIALW_FWD_PATH, data);
