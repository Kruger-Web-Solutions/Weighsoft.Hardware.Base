import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { LcdDisplayData } from '../types/lcddisplay';

export const LCD_DISPLAY_ENDPOINT = 'display';

export function readLcdDisplayData(): AxiosPromise<LcdDisplayData> {
  return AXIOS.get(LCD_DISPLAY_ENDPOINT);
}

export function updateLcdDisplayData(data: LcdDisplayData): AxiosPromise<LcdDisplayData> {
  return AXIOS.post(LCD_DISPLAY_ENDPOINT, data);
}
