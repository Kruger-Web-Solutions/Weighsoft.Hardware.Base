import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import type { WeightForwarderData } from '../types/weightForwarder';

export const WEIGHT_FORWARDER_ENDPOINT = 'weightForwarder';

export function readWeightForwarder(): AxiosPromise<WeightForwarderData> {
  return AXIOS.get(WEIGHT_FORWARDER_ENDPOINT);
}

export function updateWeightForwarder(data: WeightForwarderData): AxiosPromise<WeightForwarderData> {
  return AXIOS.post(WEIGHT_FORWARDER_ENDPOINT, data);
}
