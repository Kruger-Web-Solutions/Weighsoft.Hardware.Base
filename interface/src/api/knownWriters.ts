import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { KnownWritersData } from '../types/knownWriters';

// Display URL — full path with /rest/ for any UI that wants to show users
// where the data is coming from.
export const KNOWN_WRITERS_PATH = '/rest/writers';

// AXIOS already has baseURL='/rest/' (see endpoints.ts), so paths passed to
// each call must NOT repeat the '/rest/' prefix.
const WRITERS_RELATIVE = '/writers';

export const readKnownWriters = (): AxiosPromise<KnownWritersData> =>
  AXIOS.get(WRITERS_RELATIVE);

// Query-param based delete because backend doesn't have ASYNCWEBSERVER_REGEX enabled.
export const forgetWriter = (id: string): AxiosPromise<{ removed: boolean }> =>
  AXIOS.delete(`${WRITERS_RELATIVE}?id=${encodeURIComponent(id)}`);
