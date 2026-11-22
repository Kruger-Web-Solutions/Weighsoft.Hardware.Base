import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { VersionInfo } from '../types/version';

export const VERSION_ENDPOINT = 'version';

export function readVersion(): AxiosPromise<VersionInfo> {
  return AXIOS.get(VERSION_ENDPOINT);
}
