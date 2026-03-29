import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { WeighingData, WeighingCommand, AuditEntry } from '../types/weighing';

export function readWeighingData(): AxiosPromise<WeighingData> {
  return AXIOS.get('/weighing');
}

export function updateWeighingData(command: WeighingCommand): AxiosPromise<WeighingData> {
  return AXIOS.post('/weighing', command);
}

export function readWeighingAudit(): AxiosPromise<AuditEntry[]> {
  return AXIOS.get('/weighingAudit');
}
