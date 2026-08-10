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

/** Settings / config — requires login */
export const updateLiveWeightConfig = (state: Partial<LiveWeightState>): AxiosPromise<Partial<LiveWeightState>> =>
  AXIOS.post('/liveWeightConfig', state);

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

/**
 * CSV of the weigh log. Deliberately NOT an AXIOS call — the board streams the
 * file and sets Content-Disposition, so the browser must fetch it directly for
 * the Save dialog to appear. Pulling it through axios would land it in memory
 * as a string and lose the filename.
 */
export const liveWeightReportUrl = (): string => {
  const base = AXIOS.defaults.baseURL ?? '/rest';
  return `${base.replace(/\/$/, '')}/liveWeightReport`;
};
