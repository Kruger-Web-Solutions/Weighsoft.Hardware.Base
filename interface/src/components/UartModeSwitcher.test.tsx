import React from 'react';
import { afterEach, beforeEach, expect, jest, test } from '@jest/globals';
import { createRoot, Root } from 'react-dom/client';
import { act } from 'react-dom/test-utils';
import { SnackbarProvider } from 'notistack';

import UartModeSwitcher from './UartModeSwitcher';
import { FeaturesContext } from '../contexts/features';
import { Features } from '../types';

// time.ts imports parse-ms (ESM only), which CRA's jest does not transform; not used here.
jest.mock('../utils/time', () => ({}));

// The saved mode is writer.
jest.mock('../api/uartMode', () => ({
  readUartMode: () => Promise.resolve({ data: { mode: 'writer' } }),
  updateUartMode: () => Promise.resolve({ data: { mode: 'reader' } }),
}));

const baseFeatures: Features = {
  project: true, security: true, mqtt: true, ntp: true, ota: true, upload_firmware: true, ble: false,
};

let container: HTMLDivElement;
let root: Root;

beforeEach(() => {
  (globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;
  container = document.createElement('div');
  document.body.appendChild(container);
  root = createRoot(container);
});

afterEach(() => {
  act(() => root.unmount());
  container.remove();
});

const render = async (features: Features) => {
  await act(async () => {
    root.render(
      <SnackbarProvider>
        <FeaturesContext.Provider value={{ features }}>
          <UartModeSwitcher />
        </FeaturesContext.Provider>
      </SnackbarProvider>
    );
  });
};

const button = (name: string) => container.querySelector(`button[aria-label="${name}"]`) as HTMLButtonElement;

test('both roles say what they do in plain words', async () => {
  await render(baseFeatures);
  expect(button('reader').textContent).toBe('Reader (scale in)');
  expect(button('writer').textContent).toBe('Writer (data out)');
  // saved mode is writer, so the writer hint shows
  expect(container.textContent).toContain('Writer = data out.');
  expect(button('writer').disabled).toBe(false);
});

test('a W-Soft board is always a Reader: Writer is off and explained', async () => {
  await render({ ...baseFeatures, wsoft: true, wsoft_rs232: true });
  expect(button('writer').disabled).toBe(true);
  expect(button('reader').getAttribute('aria-pressed')).toBe('true');
  expect(container.textContent).toContain('This W-Soft board is always a Reader');
  expect(container.textContent).not.toContain('Mode has changed');
});
