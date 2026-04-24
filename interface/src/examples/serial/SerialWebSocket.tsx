import React, { FC, useState, useEffect, useRef } from 'react';
import { Typography, Box, Button } from '@mui/material';
import { SectionContent } from '../../components';
import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { useWs } from '../../utils';
import { SerialData } from '../../types/serial';
import { formatSerialTimestamp, sanitizeLastLineForDisplay } from './formatSerialTimestamp';

export const SERIAL_WEBSOCKET_URL = WEB_SOCKET_ROOT + "serial";

const SerialWebSocket: FC = () => {
  const [lines, setLines] = useState<Array<{
    text: string;
    timestamp: number;
    weight: string;
  }>>([]);
  const scrollRef = useRef<HTMLDivElement>(null);

  const { data } = useWs<SerialData>(SERIAL_WEBSOCKET_URL);

  useEffect(() => {
    if (data?.last_line) {
      setLines((prev) => [...prev, {
        text: data.last_line,
        timestamp: data.timestamp,
        weight: data.weight ?? ''
      }].slice(-100));
      
      // Auto-scroll to bottom
      if (scrollRef.current) {
        scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
      }
    }
  }, [data]);

  return (
    <SectionContent title='Live Stream (WebSocket)' titleGutter>
      <Typography variant="body2" paragraph>
        Real-time serial data streaming via WebSocket. Shows last 100 lines.
      </Typography>
      
      <Button onClick={() => setLines([])} sx={{ mb: 2 }}>
        Clear
      </Button>
      
      <Box 
        ref={scrollRef}
        sx={{ 
          fontFamily: 'monospace',
          fontSize: '0.85rem',
          bgcolor: 'background.paper', 
          p: 2, 
          borderRadius: 1,
          height: 400,
          overflow: 'auto'
        }}
      >
        {lines.length === 0 ? (
          <Typography color="text.secondary">Waiting for data...</Typography>
        ) : (
          lines.map((line, idx) => (
            <Box key={idx} sx={{ mb: 0.5 }}>
              <Typography
                component="span"
                variant="caption"
                color="text.secondary"
                sx={{ mr: 1 }}
              >
                [{formatSerialTimestamp(line.timestamp)}]
              </Typography>
              <Typography component="span">
                {sanitizeLastLineForDisplay(line.text)}
              </Typography>
              {line.weight !== '' && (
                <Typography component="span" color="primary" sx={{ ml: 1 }}>
                  → {line.weight}
                </Typography>
              )}
            </Box>
          ))
        )}
      </Box>
    </SectionContent>
  );
};

export default SerialWebSocket;
