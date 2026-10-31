import { AxiosPromise } from 'axios';

import { AXIOS, WEB_SOCKET_ROOT } from '../../api/endpoints';

import {
  WSoftDjbRequest, WSoftDjbResult, WSoftHubPairResult, WSoftHubStatus, WSoftSavedList, WSoftSaveResult, WSoftSettings,
  WSoftStatus
} from './types';

export const WSOFT_STATUS_WEBSOCKET_URL = WEB_SOCKET_ROOT + 'wsoftStatus';

export const readWSoftStatus = (): AxiosPromise<WSoftStatus> => AXIOS.get('/wsoftStatus');

/** Admin only. */
export const readWSoftSettings = (): AxiosPromise<WSoftSettings> => AXIOS.get('/wsoftSettings');

/** Admin only. The board rejects a bad topic or name with 400 and keeps the old values. */
export const updateWSoftSettings = (settings: WSoftSettings): AxiosPromise<WSoftSettings> =>
  AXIOS.post('/wsoftSettings', settings);

/** Admin only. Never returns the pairing secret. */
export const readWSoftHub = (): AxiosPromise<WSoftHubStatus> => AXIOS.get('/wsoftHub');

/** Admin only. 400 with a message for a bad code; on 200 the box restarts to use it. */
export const pairWSoftHub = (code: string): AxiosPromise<WSoftHubPairResult> =>
  AXIOS.post('/wsoftHub', { code });

/** Admin only. Forgets the Hub link; the box restarts. */
export const unpairWSoftHub = (): AxiosPromise<void> => AXIOS.delete('/wsoftHub');

/**
 * Admin only. One DJB register read or write over RS485. Errors come back as 400/409/504 with
 * the same JSON body ({ ok: false, error }).
 */
/**
 * Any logged-in user. Same as SAVE on the screen: 200 when saved, 409 with a reason when not
 * (moving, empty, already saved, ...), 503 when the screen did not answer in time.
 */
export const saveWeighing = (): AxiosPromise<WSoftSaveResult> => AXIOS.post('/wsoftSave');

/** Any logged-in user. The last 10 saved weighings, newest first. */
export const readSavedWeighings = (): AxiosPromise<WSoftSavedList> => AXIOS.get('/wsoftSave');

export const djb = (request: WSoftDjbRequest): AxiosPromise<WSoftDjbResult> => AXIOS.post('/djb', request);
