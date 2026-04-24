import { useCallback, useEffect, useRef, useState } from 'react';
import Sockette from 'sockette';
import { debounce } from 'lodash';

import { addAccessTokenParameter } from '../api/authentication';

interface WebSocketIdMessage {
  type: "id";
  id: string;
}

interface WebSocketPayloadMessage<D> {
  type: "payload";
  origin_id: string;
  payload: D;
}

export type WebSocketMessage<D> = WebSocketIdMessage | WebSocketPayloadMessage<D>;

export const useWs = <D>(wsUrl: string, wsThrottle: number = 100) => {

  const ws = useRef<Sockette>();
  const clientId = useRef<string>();

  const [connected, setConnected] = useState<boolean>(false);
  const [data, setData] = useState<D>();
  const [transmit, setTransmit] = useState<boolean>();
  const [clear, setClear] = useState<boolean>();

  const onMessage = useCallback((event: MessageEvent) => {
    const rawData = event.data;
    if (typeof rawData === 'string' || rawData instanceof String) {
      const message = JSON.parse(rawData as string) as WebSocketMessage<D>;
      switch (message.type) {
        case "id":
          clientId.current = message.id;
          break;
        case "payload": {
          // Server sends origin_id "websocket" (initial snapshot), "serial_hw", "mqtt", etc. — not the client's
          // "websocket:<id>" except when echoing our own WS write. Ignore only that self-echo; apply all other payloads.
          const applies = !clientId.current || message.origin_id !== clientId.current;
          const p = message.payload as Record<string, unknown> | undefined;
          const ll = typeof p?.last_line === 'string' ? p.last_line : '';
          // #region agent log
          if (wsUrl.includes('serial')) {
            fetch('http://127.0.0.1:7717/ingest/0bfa9906-aaa9-4a5a-9ff1-54d549900d98', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json', 'X-Debug-Session-Id': '76d0d7' },
              body: JSON.stringify({
                sessionId: '76d0d7',
                hypothesisId: 'H-WS',
                location: 'useWs.ts:payload',
                message: 'ws_serial_payload',
                data: {
                  origin_id: message.origin_id,
                  clientId: clientId.current ?? null,
                  applies,
                  lastLineLen: ll.length,
                  ts: typeof p?.timestamp === 'number' ? p.timestamp : -1,
                },
                timestamp: Date.now(),
              }),
            }).catch(() => {});
          }
          // #endregion
          if (applies) {
            setData(message.payload);
          }
          break;
        }
      }
    }
  }, [wsUrl]);

  const doSaveData = useCallback((newData: D, clearData: boolean = false) => {
    if (!ws.current) {
      return;
    }
    if (clearData) {
      setData(undefined);
    }
    ws.current.json(newData);
  }, []);

  const saveData = useRef(debounce(doSaveData, wsThrottle));

  const updateData = (newData: React.SetStateAction<D | undefined>, transmitData: boolean = true, clearData: boolean = false) => {
    setData(newData);
    setTransmit(transmitData);
    setClear(clearData);
  };

  useEffect(() => {
    if (!transmit) {
      return;
    }
    data && saveData.current(data, clear);
    setTransmit(false);
    setClear(false);
  }, [doSaveData, data, transmit, clear]);

  useEffect(() => {
    const instance = new Sockette(addAccessTokenParameter(wsUrl), {
      onmessage: onMessage,
      onopen: () => {
        setConnected(true);
      },
      onclose: () => {
        clientId.current = undefined;
        setConnected(false);
        setData(undefined);
      },
    });
    ws.current = instance;
    return instance.close;
  }, [wsUrl, onMessage]);

  return { connected, data, updateData } as const;
};
