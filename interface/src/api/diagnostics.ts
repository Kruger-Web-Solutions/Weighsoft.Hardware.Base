import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { DiagnosticsData } from '../types/diagnostics';

export const DIAGNOSTICS_ENDPOINT = 'diagnostics';

export function readDiagnostics(): AxiosPromise<DiagnosticsData> {
  return AXIOS.get(DIAGNOSTICS_ENDPOINT);
}

export function updateDiagnostics(data: Record<string, any>): AxiosPromise<DiagnosticsData> {
  return AXIOS.post(DIAGNOSTICS_ENDPOINT, data);
}
