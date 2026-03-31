import { AxiosPromise } from 'axios';

import { AXIOS } from '../../api/endpoints';
import { GrowboxState, GrowboxStateUpdate, GrowboxSettings, GrowboxAutomationSettings } from './types';

export function readGrowboxState(): AxiosPromise<GrowboxState> {
  return AXIOS.get('/growbox');
}

export function updateGrowboxState(state: GrowboxState | GrowboxStateUpdate): AxiosPromise<GrowboxState> {
  return AXIOS.post('/growbox', state);
}

export function readGrowboxSettings(): AxiosPromise<GrowboxSettings> {
  return AXIOS.get('/growboxSettings');
}

export function updateGrowboxSettings(settings: GrowboxSettings): AxiosPromise<GrowboxSettings> {
  return AXIOS.post('/growboxSettings', settings);
}

export function readGrowboxAutomation(): AxiosPromise<GrowboxAutomationSettings> {
  return AXIOS.get('/growboxAutomation');
}

export function updateGrowboxAutomation(settings: GrowboxAutomationSettings): AxiosPromise<GrowboxAutomationSettings> {
  return AXIOS.post('/growboxAutomation', settings);
}
