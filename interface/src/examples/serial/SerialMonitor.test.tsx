import React from 'react';
import { afterEach, beforeEach, expect, jest, test } from '@jest/globals';
import { createRoot, Root } from 'react-dom/client';
import { act } from 'react-dom/test-utils';
import { SnackbarProvider } from 'notistack';

import SerialMonitor from './SerialMonitor';
import { FeaturesContext } from '../../contexts/features';
import { Features } from '../../types';
import { LayoutContext } from '../../components/layout/context';

// time.ts imports parse-ms (ESM only), which CRA's jest does not transform; not used here.
jest.mock('../../utils/time', () => ({}));
jest.mock('../../api/uartMode', () => ({
  readUartMode: () => Promise.resolve({ data: { mode: 'reader' } }),
  updateUartMode: () => Promise.resolve({ data: { mode: 'reader' } }),
}));
// A W-Soft board whose weight comes over RS485 (the weighing card).
jest.mock('../../project/wsoft/api', () => ({ readWSoftStatus: () => Promise.resolve({ data: { source: 'rs485' } }) }));

const wsoftFeatures: Features = {
  project: true, security: true, mqtt: true, ntp: true, ota: false, upload_firmware: false, ble: false,
  wsoft: true, wsoft_rs232: true,
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

test('a W-Soft board on RS485 says the Serial reader is off, and still offers Weigh / Writer', async () => {
  // react-dom's own act around createRoot().render; the rule mistakes it for Testing Library.
  // eslint-disable-next-line testing-library/no-unnecessary-act
  await act(async () => {
    root.render(
      <SnackbarProvider>
        <LayoutContext.Provider value={{ title: '', setTitle: () => {} }}>
          <FeaturesContext.Provider value={{ features: wsoftFeatures }}>
            <SerialMonitor />
          </FeaturesContext.Provider>
        </LayoutContext.Provider>
      </SnackbarProvider>
    );
  });
  expect(container.textContent).toContain('reads the scale over RS485');
  expect(container.textContent).toContain('W-Soft > Settings');
  expect(container.textContent).toContain('Weigh (reads the scale)');
  expect(container.textContent).toContain('Writer (follows a weigh board)');
});
