import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { UartModeData } from '../types/uartMode';

export const UART_MODE_ENDPOINT = 'uartMode';

export function readUartMode(): AxiosPromise<UartModeData> {
  return AXIOS.get(UART_MODE_ENDPOINT);
}

export function updateUartMode(data: Record<string, any>): AxiosPromise<UartModeData> {
  return AXIOS.post(UART_MODE_ENDPOINT, data);
}
