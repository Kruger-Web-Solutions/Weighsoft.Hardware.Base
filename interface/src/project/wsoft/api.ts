import { AxiosPromise } from 'axios';

import { AXIOS, WEB_SOCKET_ROOT } from '../../api/endpoints';

import { WSoftSettings, WSoftStatus } from './types';

export const WSOFT_STATUS_WEBSOCKET_URL = WEB_SOCKET_ROOT + 'wsoftStatus';

export const readWSoftStatus = (): AxiosPromise<WSoftStatus> => AXIOS.get('/wsoftStatus');

/** Admin only. */
export const readWSoftSettings = (): AxiosPromise<WSoftSettings> => AXIOS.get('/wsoftSettings');

/** Admin only. The board rejects a bad topic or name with 400 and keeps the old values. */
export const updateWSoftSettings = (settings: WSoftSettings): AxiosPromise<WSoftSettings> =>
  AXIOS.post('/wsoftSettings', settings);
