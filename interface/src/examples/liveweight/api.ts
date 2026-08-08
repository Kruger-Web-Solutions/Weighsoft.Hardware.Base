import { AxiosPromise } from 'axios';

import { AXIOS } from '../../api/endpoints';

import { LiveWeightState } from './types';

export const readLiveWeight = (): AxiosPromise<LiveWeightState> => AXIOS.get('/liveWeight');

export const updateLiveWeight = (state: Partial<LiveWeightState>): AxiosPromise<LiveWeightState> =>
  AXIOS.post('/liveWeight', state);
