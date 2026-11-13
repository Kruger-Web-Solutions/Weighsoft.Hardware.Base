import { expect, test } from '@jest/globals';

import { normalizeReaderSourceUrl, normalizeReaderSourceUrlForPersistence } from './serialWriterSource';

test('a Serial reader address becomes its /ws/serial feed, as before', () => {
  expect(normalizeReaderSourceUrl('http://192.168.2.60/')).toBe('ws://192.168.2.60/ws/serial');
  expect(normalizeReaderSourceUrl('192.168.2.60')).toBe('ws://192.168.2.60/ws/serial');
});

test('a W-Soft weigh board address becomes its weight feed', () => {
  expect(normalizeReaderSourceUrl('http://192.168.2.60/', 'wsoft')).toBe('ws://192.168.2.60/ws/wsoftFeed');
  expect(normalizeReaderSourceUrlForPersistence('192.168.2.60', 1, 'wsoft')).toBe('http://192.168.2.60/rest/wsoftFeed');
  // a typed path is kept, so a W-Soft Writer can still follow a plain Serial reader
  expect(normalizeReaderSourceUrl('ws://192.168.2.60/ws/serial', 'wsoft')).toBe('ws://192.168.2.60/ws/serial');
});
