import { AxiosPromise } from 'axios';

import { AXIOS, WEB_SOCKET_ROOT } from '../../api/endpoints';

import { WSoftHub, WSoftHubAction, WSoftSave, WSoftSettings, WSoftStatus } from './types';

export const WSOFT_STATUS_WEBSOCKET_URL = WEB_SOCKET_ROOT + 'wsoftStatus';

export const readWSoftStatus = (): AxiosPromise<WSoftStatus> => AXIOS.get('/wsoftStatus');

/** Admin only. */
export const readWSoftSettings = (): AxiosPromise<WSoftSettings> => AXIOS.get('/wsoftSettings');

/** Admin only. The board rejects a bad topic or name with 400 and keeps the old values. */
export const updateWSoftSettings = (settings: WSoftSettings): AxiosPromise<WSoftSettings> =>
  AXIOS.post('/wsoftSettings', settings);

/** Admin only. Hub link: address, ids and state. Never the pairing key. */
export const readWSoftHub = (): AxiosPromise<WSoftHub> => AXIOS.get('/wsoftHub');

/** Admin only. Connect with a pairing key, or disconnect. The board restarts on success. */
export const updateWSoftHub = (action: WSoftHubAction): AxiosPromise<WSoftHub> => AXIOS.post('/wsoftHub', action);

/** Logged-in users. Weighings waiting for the Hub and the last one saved. */
export const readWSoftSave = (): AxiosPromise<WSoftSave> => AXIOS.get('/wsoftSave');

/** Logged-in users. Saves the load on the scale now; 409 with `result` when it cannot. */
export const saveWSoftWeighing = (): AxiosPromise<WSoftSave> => AXIOS.post('/wsoftSave');
