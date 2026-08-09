import { AxiosPromise } from 'axios';

import { AXIOS } from '../../api/endpoints';

import { LiveWeightState } from './types';

export interface LiveWeightProductEntry {
  plu: string;
  product: string;
  unit: string;
}

export interface LiveWeightProductsResponse {
  max: number;
  count: number;
  products: LiveWeightProductEntry[];
}

export interface LiveWeightTransaction {
  t: number;
  reason: string;
  plu: string;
  product: string;
  weight: string;
  count: number;
  total: string;
  unit: string;
}

export interface LiveWeightTransactionsResponse {
  max: number;
  count: number;
  transactions: LiveWeightTransaction[];
}

export const readLiveWeight = (): AxiosPromise<LiveWeightState> => AXIOS.get('/liveWeight');

export const updateLiveWeight = (state: Partial<LiveWeightState>): AxiosPromise<LiveWeightState> =>
  AXIOS.post('/liveWeight', state);

export const readLiveWeightProducts = (): AxiosPromise<LiveWeightProductsResponse> =>
  AXIOS.get('/liveWeightProducts');

export const upsertLiveWeightProduct = (
  entry: LiveWeightProductEntry
): AxiosPromise<LiveWeightProductsResponse> =>
  AXIOS.post('/liveWeightProducts', { action: 'upsert', ...entry });

export const selectLiveWeightProduct = (plu: string): AxiosPromise<LiveWeightProductsResponse> =>
  AXIOS.post('/liveWeightProducts', { action: 'select', plu });

export const deleteLiveWeightProduct = (plu: string): AxiosPromise<LiveWeightProductsResponse> =>
  AXIOS.post('/liveWeightProducts', { action: 'delete', plu });

export const readLiveWeightTransactions = (): AxiosPromise<LiveWeightTransactionsResponse> =>
  AXIOS.get('/liveWeightTransactions');
