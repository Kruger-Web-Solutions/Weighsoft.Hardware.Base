import { AxiosPromise } from 'axios';

import { AXIOS, WEB_SOCKET_ROOT } from '../../api/endpoints';

import {
  WSoftDjbReply, WSoftDjbRequest, WSoftHub, WSoftLoadcell, WSoftLoadcellRequest, WSoftHubAction, WSoftPinReply, WSoftPinRequest,
  WSoftProductAction, WSoftProducts, WSoftRelayBoard, WSoftRelayStatus, WSoftScaleSetup, WSoftScaleSetupRequest,
  WSoftSave, WSoftSettings, WSoftStatus, WSoftSwitches, WSoftSwitchRequest,
  WSoftWifiUpdate, WSoftWifiUpdateRequest
} from './types';
import { WsoftScreenModel } from './gen/wsoftScreen';

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

/** Logged-in users. The words on the board's screen now (WsoftUiModel in the firmware). */
export const readWSoftScreen = (): AxiosPromise<WsoftScreenModel> => AXIOS.get('/wsoftScreen');

/** Logged-in users. A tap on the board's screen, in its 320 x 240 layout. */
export const tapWSoftScreen = (point: { x: number; y: number }): AxiosPromise<void> => AXIOS.post('/wsoftScreen', point);

/** Logged-in users. Approval and Auto save as the board holds them, plus the approval log. */
export const readWSoftSwitches = (): AxiosPromise<WSoftSwitches> => AXIOS.get('/wsoftSwitches');

/** Admin only. Switches Approval or Auto save. 409 with a code when it cannot. */
export const updateWSoftSwitch = (request: WSoftSwitchRequest): AxiosPromise<WSoftSwitches> =>
  AXIOS.post('/wsoftSwitches', request);

/** Admin only. Sets the screen Approval PIN (4 to 6 digits). The board never sends it back. */
export const updateWSoftPin = (request: WSoftPinRequest): AxiosPromise<WSoftPinReply> => AXIOS.post('/wsoftPin', request);

/** Admin only. The Wi-Fi update switch. Never the password. */
export const readWSoftWifiUpdate = (): AxiosPromise<WSoftWifiUpdate> => AXIOS.get('/wsoftWifiUpdate');

/** Admin only. Switches Wi-Fi update or sets the board's own update password. 400/409 with a code when it cannot. */
export const updateWSoftWifiUpdate = (request: WSoftWifiUpdateRequest): AxiosPromise<WSoftWifiUpdate> =>
  AXIOS.post('/wsoftWifiUpdate', request);

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

/** Admin only. The load cell on an HX711 board: raw count, weight and calibration. */
export const readWSoftLoadcell = (): AxiosPromise<WSoftLoadcell> => AXIOS.get('/wsoftLoadcell');

/** Admin only. Set zero, set the test weight or set the display step. 400/409 with a code and a sentence when it cannot. */
export const updateWSoftLoadcell = (request: WSoftLoadcellRequest): AxiosPromise<WSoftLoadcell> =>
  AXIOS.post('/wsoftLoadcell', request);
