import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { SerialWriterData, SerialWriterForwarderData } from '../types/serialWriter';

// Display URLs (used for curl examples, endpoint info cards). Full path so users
// can copy them into terminals/tools.
export const SERIALW_REST_PATH = '/rest/serialWriter';
export const SERIALW_SEND_PATH = '/rest/serialWriter/send';
export const SERIALW_FWD_PATH  = '/rest/serialWriterSource';

// AXIOS already has baseURL='/rest/' (see endpoints.ts), so the path passed to
// each call must NOT include the '/rest/' prefix — otherwise it doubles up.
const REST_RELATIVE = '/serialWriter';
const SEND_RELATIVE = '/serialWriter/send';
const FWD_RELATIVE  = '/serialWriterSource';

export const readSerialWriter = (): AxiosPromise<SerialWriterData> =>
  AXIOS.get(REST_RELATIVE);

export const updateSerialWriter = (data: Partial<SerialWriterData>): AxiosPromise<SerialWriterData> =>
  AXIOS.post(REST_RELATIVE, data);

export const sendWriterMessage = (data: string): AxiosPromise<{ sent: boolean }> =>
  AXIOS.post(SEND_RELATIVE, { data });

export const readWriterSource = (): AxiosPromise<SerialWriterForwarderData> =>
  AXIOS.get(FWD_RELATIVE);

export const updateWriterSource = (data: Partial<SerialWriterForwarderData>): AxiosPromise<SerialWriterForwarderData> =>
  AXIOS.post(FWD_RELATIVE, data);
