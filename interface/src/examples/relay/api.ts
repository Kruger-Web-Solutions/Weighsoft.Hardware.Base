import { AxiosPromise } from 'axios';

import { AXIOS } from '../../api/endpoints';

import { RelayBoardState, RelayBoardStatus } from './types';

export function readRelayBoard(): AxiosPromise<RelayBoardState> {
  return AXIOS.get('/relayBoard');
}

export function updateRelayBoard(state: RelayBoardState): AxiosPromise<RelayBoardState> {
  return AXIOS.post('/relayBoard', state);
}

export function readRelayBoardStatus(): AxiosPromise<RelayBoardStatus> {
  return AXIOS.get('/relayBoardStatus');
}
