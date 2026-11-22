import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { DiscoveredDevicesData } from '../types/discovered';

// Display URL kept as a full /rest/... path for any UI bits that show it.
export const DISCOVERED_PATH = '/rest/discovered';

// AXIOS already prepends baseURL='/rest/', so the relative path here must NOT
// include it (otherwise it doubles up — see the earlier /rest/rest/* incident).
const DISCOVERED_RELATIVE = '/discovered';

export const readDiscovered = (): AxiosPromise<DiscoveredDevicesData> =>
  AXIOS.get(DISCOVERED_RELATIVE);
