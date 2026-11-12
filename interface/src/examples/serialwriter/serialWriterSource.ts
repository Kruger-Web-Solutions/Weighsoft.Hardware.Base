import { SerialWriterForwarderData } from '../../types/serialWriter';

/** Shown when the Writer cannot reach its Reader and the board gave no error text. */
export const READER_UNREACHABLE_HINT = 'Check the Reader is on, on the same Wi-Fi, and the address is right.';

/** Which feed a Writer reads: a Serial reader's raw lines, or a W-Soft weigh board's weight. */
export type ReaderFeed = 'serial' | 'wsoft';
const feedPath = (feed: ReaderFeed, kind: 'ws' | 'rest') =>
  `/${kind}/${feed === 'wsoft' ? 'wsoftFeed' : 'serial'}`;

/** Fields the weight format can use (wsoft_output_format.h on the board). */
export const OUTPUT_FORMAT_DEFAULT = '{line}';
export const OUTPUT_FORMAT_MAX = 63;

export type ReaderConnectionState = 'not_configured' | 'connected' | 'reconnecting' | 'disabled' | 'error';

export const normalizeReaderSourceUrl = (value: string, feed: ReaderFeed = 'serial'): string => {
  const trimmed = value.trim();
  if (!trimmed) {
    return '';
  }

  const withScheme = /^[a-z]+:\/\//i.test(trimmed) ? trimmed : `ws://${trimmed}`;

  try {
    const url = new URL(withScheme);
    if (url.protocol === 'http:' || url.protocol === 'https:') {
      url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
      url.pathname = feedPath(feed, 'ws');
      url.search = '';
      url.hash = '';
      return url.toString().replace(/\/$/, '');
    }
    if (url.protocol === 'ws:' || url.protocol === 'wss:') {
      if (!url.pathname || url.pathname === '/') {
        url.pathname = feedPath(feed, 'ws');
      }
      url.search = '';
      url.hash = '';
      return url.toString().replace(/\/$/, '');
    }
  } catch (_) {
    return trimmed;
  }

  return trimmed;
};

/** Persisted URL must match connection_method: HTTP mode keeps http(s) and defaults path to /rest/serial. */
export const normalizeReaderSourceUrlForPersistence = (
  value: string,
  connectionMethod: 0 | 1,
  feed: ReaderFeed = 'serial',
): string => {
  if (connectionMethod === 1) {
    const trimmed = value.trim();
    if (!trimmed) {
      return '';
    }
    const withScheme = /^[a-z]+:\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
    try {
      const url = new URL(withScheme);
      if (url.protocol !== 'http:' && url.protocol !== 'https:') {
        url.protocol = 'http:';
      }
      if (!url.pathname || url.pathname === '/') {
        url.pathname = feedPath(feed, 'rest');
      }
      url.search = '';
      url.hash = '';
      return url.toString().replace(/\/$/, '');
    } catch (_) {
      return trimmed;
    }
  }
  return normalizeReaderSourceUrl(value, feed);
};

export const readerHostFromSourceUrl = (value: string): string => {
  if (!value) {
    return '';
  }
  try {
    return new URL(value).host;
  } catch (_) {
    return value.replace(/^wss?:\/\//, '').replace(/^https?:\/\//, '').split('/')[0];
  }
};

export const readerConnectionState = (source: SerialWriterForwarderData | null): ReaderConnectionState => {
  if (!source || !source.source_url) {
    return 'not_configured';
  }
  if (!source.enabled) {
    return 'disabled';
  }
  if (source.connected) {
    return 'connected';
  }
  if (source.last_error) {
    return 'error';
  }
  return 'reconnecting';
};

export const connectionMethodLabel = (method: SerialWriterForwarderData['connection_method']): string =>
  method === 0 ? 'WebSocket live stream' : 'HTTP polling';

export const serialSummary = (baudRate: number, dataBits: number, parity: number, stopBits: number, lineEnding: number): string => {
  const parityLabel = parity === 1 ? 'E' : parity === 2 ? 'O' : 'N';
  const lineEndingLabel = ['none', 'CR', 'LF', 'CRLF'][lineEnding] || 'unknown';
  return `${baudRate} ${dataBits}${parityLabel}${stopBits} · ${lineEndingLabel} line ending`;
};
