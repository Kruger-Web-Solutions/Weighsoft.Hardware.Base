import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { KnownWritersData } from '../types/knownWriters';

export const KNOWN_WRITERS_PATH = '/rest/writers';

export const readKnownWriters = (): AxiosPromise<KnownWritersData> =>
  AXIOS.get(KNOWN_WRITERS_PATH);

// Query-param based delete because backend doesn't have ASYNCWEBSERVER_REGEX enabled.
export const forgetWriter = (id: string): AxiosPromise<{ removed: boolean }> =>
  AXIOS.delete(`${KNOWN_WRITERS_PATH}?id=${encodeURIComponent(id)}`);
