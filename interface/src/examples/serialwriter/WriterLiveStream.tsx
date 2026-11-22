import React, { FC, useEffect, useRef, useState } from 'react';
import { Box, Card, CardContent, Chip, Button, Typography, Alert, TextField } from '@mui/material';
import { SectionContent } from '../../components';
import { ACCESS_TOKEN } from '../../api/endpoints';
import { sendWriterMessage } from '../../api/serialWriter';

interface TxEvent { ts: number; source: string; data: string; bytes: number; }

const sourceColor = (s: string): 'primary' | 'info' | 'warning' | 'success' | 'secondary' | 'default' => {
  switch (s) {
    case 'reader': return 'primary';
    case 'manual': return 'info';
    case 'rest':   return 'warning';
    case 'ws':     return 'success';
    case 'mqtt':   return 'secondary';
    default:       return 'default';
  }
};

const MAX_LINES = 100;

const sourceTagFromCode = (n: number): string =>
  ['none', 'manual', 'rest', 'ws', 'mqtt', 'reader'][n] || 'none';

const WriterLiveStream: FC = () => {
  const [events, setEvents] = useState<TxEvent[]>([]);
  const [connected, setConnected] = useState(false);
  const [outgoing, setOutgoing] = useState('');
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const accessToken = localStorage.getItem(ACCESS_TOKEN);
    const tokenQuery = accessToken ? `?${ACCESS_TOKEN}=${encodeURIComponent(accessToken)}` : '';
    const ws = new WebSocket(`${proto}//${window.location.host}/ws/serialWriter${tokenQuery}`);
    wsRef.current = ws;
    ws.onopen = () => setConnected(true);
    ws.onclose = () => setConnected(false);
    ws.onmessage = (m) => {
      try {
        const obj = JSON.parse(m.data);
        // The Writer's WebSocketTxRx broadcasts the full SerialWriterState on every update.
        // last_sent + last_sent_source give us the data we need to log.
        if (obj.last_sent && typeof obj.last_sent === 'string') {
          const sourceCode = typeof obj.last_sent_source === 'number' ? obj.last_sent_source : 0;
          setEvents((prev) => {
            const next: TxEvent = {
              ts: Date.now(),
              source: sourceTagFromCode(sourceCode),
              data: obj.last_sent,
              bytes: obj.last_sent.length,
            };
            // Avoid duplicates if the same payload comes through multiple state syncs
            const isDuplicateOfHead =
              prev[0] &&
              prev[0].data === next.data &&
              prev[0].source === next.source &&
              Math.abs(prev[0].ts - next.ts) < 200;
            if (isDuplicateOfHead) return prev;
            return [next, ...prev].slice(0, MAX_LINES);
          });
        }
      } catch { /* ignore non-JSON frames */ }
    };
    return () => ws.close();
  }, []);

  const sendThroughWs = () => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    const text = outgoing.trim();
    if (text.length === 0) return;
    // Send via the REST endpoint — simpler than WS inbound auth for v1.
    sendWriterMessage(text).catch(() => {});
    setOutgoing('');
  };

  return (
    <SectionContent title="Live Stream" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>Every message that left this Writer's serial port — by anyone.</Alert>

      <Alert severity={connected ? 'success' : 'warning'} sx={{ mb: 2 }}>
        {connected ? 'Connected to the live activity feed.' : 'Connecting...'}
      </Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="subtitle2" gutterBottom>Send through this socket</Typography>
          <Box display="flex" gap={1}>
            <TextField
              fullWidth
              size="small"
              value={outgoing}
              onChange={(e) => setOutgoing(e.target.value)}
              onKeyDown={(e) => { if (e.key === 'Enter') sendThroughWs(); }}
              placeholder="type message..."
            />
            <Button
              variant="contained"
              disabled={!connected || outgoing.trim().length === 0}
              onClick={sendThroughWs}
            >
              Send
            </Button>
          </Box>
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Box display="flex" justifyContent="space-between" alignItems="center" mb={1}>
            <Typography variant="subtitle2">Recent activity (last {MAX_LINES})</Typography>
            <Button size="small" onClick={() => setEvents([])}>Clear</Button>
          </Box>
          <Box
            sx={{
              maxHeight: 320,
              overflow: 'auto',
              fontFamily: 'monospace',
              fontSize: 12,
              border: '1px solid #eee',
              p: 1,
              borderRadius: 1,
            }}
          >
            {events.length === 0
              ? <Typography color="text.secondary">Waiting for activity...</Typography>
              : events.map((e, i) => (
                <Box key={i} display="flex" gap={1} alignItems="center" mb={0.5}>
                  <Typography component="span" color="text.secondary" variant="caption">
                    [{new Date(e.ts).toLocaleTimeString()}]
                  </Typography>
                  <Chip
                    size="small"
                    color={sourceColor(e.source)}
                    label={e.source.toUpperCase()}
                    sx={{ minWidth: 70 }}
                  />
                  <Typography component="span" sx={{ wordBreak: 'break-all' }}>{e.data}</Typography>
                  <Typography component="span" color="text.secondary" variant="caption">
                    {`→ ${e.bytes} bytes`}
                  </Typography>
                </Box>
              ))}
          </Box>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterLiveStream;
