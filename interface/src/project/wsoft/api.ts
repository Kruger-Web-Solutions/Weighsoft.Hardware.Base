import { AxiosPromise } from 'axios';

import { AXIOS, WEB_SOCKET_ROOT } from '../../api/endpoints';

import {
  WSoftDjbReply, WSoftDjbRequest, WSoftHub, WSoftHubAction, WSoftPinReply, WSoftPinRequest, WSoftSave,
  WSoftProductAction, WSoftProducts, WSoftRelayBoard, WSoftRelayStatus, WSoftScaleSetup, WSoftScaleSetupRequest, WSoftSettings, WSoftStatus, WSoftSwitches, WSoftSwitchRequest
} from './types';

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

/** Logged-in users. Approval and Auto save as the board holds them, plus the approval log. */
export const readWSoftSwitches = (): AxiosPromise<WSoftSwitches> => AXIOS.get('/wsoftSwitches');

/** Admin only. Switches Approval or Auto save. 409 with a code when it cannot. */
export const updateWSoftSwitch = (request: WSoftSwitchRequest): AxiosPromise<WSoftSwitches> =>
  AXIOS.post('/wsoftSwitches', request);

/** Admin only. Sets the screen Approval PIN (4 to 6 digits). The board never sends it back. */
export const updateWSoftPin = (request: WSoftPinRequest): AxiosPromise<WSoftPinReply> => AXIOS.post('/wsoftPin', request);

/** Logged-in users. The RS485 scale setup kept on the board and the last card values read. */
export const readWSoftScaleSetup = (): AxiosPromise<WSoftScaleSetup> => AXIOS.get('/wsoftScaleSetup');

/** Admin only. Saves the board part of the scale setup. 409 with a code when it cannot. */
export const updateWSoftScaleSetup = (request: WSoftScaleSetupRequest): AxiosPromise<WSoftScaleSetup> =>
  AXIOS.post('/wsoftScaleSetup', request);

/** Admin only. One weighing card register read or write, run by the screen task. */
export const djb = (request: WSoftDjbRequest): AxiosPromise<WSoftDjbReply> => AXIOS.post('/djb', { ...request, channel: 1 });

// Hi-Lo relays (boards built with relay outputs). Both need a login.
export const readWSoftRelayStatus = (): AxiosPromise<WSoftRelayStatus> => AXIOS.get('/relayBoardStatus');

export const updateWSoftRelays = (relays: Partial<WSoftRelayBoard>): AxiosPromise<WSoftRelayBoard> =>
  AXIOS.post('/relayBoard', relays);

/** Logged-in users. Hub products and board products, and the one in use. */
export const readWSoftProducts = (): AxiosPromise<WSoftProducts> => AXIOS.get('/wsoftProducts');

/** Admin only. Add, edit or delete a board product, or pick the product in use. 400/409 with a code when it cannot. */
export const updateWSoftProducts = (action: WSoftProductAction): AxiosPromise<WSoftProducts> =>
  AXIOS.post('/wsoftProducts', action);
