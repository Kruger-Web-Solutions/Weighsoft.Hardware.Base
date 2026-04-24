import React, { FC, useEffect } from 'react';
import { Typography, Box } from '@mui/material';
import { SectionContent, FormLoader } from '../../components';
import { useRest } from '../../utils';
import { readSerialData } from '../../api/serial';
import { SerialData } from '../../types/serial';
import { formatSerialTimestamp, sanitizeLastLineForDisplay } from './formatSerialTimestamp';

const SerialRest: FC = () => {
  const { data, loadData, errorMessage } = useRest<SerialData>({ read: readSerialData });

  // Poll every 2 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      loadData();
    }, 2000);
    return () => clearInterval(interval);
  }, [loadData]);

  return (
    <SectionContent title='REST View (Last Line)' titleGutter>
      <Typography variant="body2" paragraph>
        This view polls the REST API every 2 seconds to fetch the last received line.
        For real-time streaming, use the WebSocket view.
      </Typography>
      
      {!data ? (
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      ) : (
        <Box
          sx={{
            fontFamily: 'monospace',
            bgcolor: 'background.paper',
            p: 2,
            borderRadius: 1,
            minHeight: 100
          }}
        >
          {data.last_line ? (
            <>
              <Typography variant="caption" color="text.secondary">
                Timestamp: {formatSerialTimestamp(data.timestamp)}
              </Typography>
              <Typography sx={{ display: 'block', mt: 0.5 }}>
                Raw: {sanitizeLastLineForDisplay(data.last_line)}
              </Typography>
              {data.weight !== undefined && data.weight !== '' ? (
                <Typography sx={{ mt: 1 }} color="primary">
                  Extracted weight: {data.weight}
                </Typography>
              ) : (
                data.last_line && (
                  <Typography variant="caption" color="text.secondary" sx={{ mt: 1, display: 'block' }}>
                    No weight extracted (set regex in Configuration to extract a value)
                  </Typography>
                )
              )}
            </>
          ) : (
            <>
              <Typography color="text.secondary">No data received yet</Typography>
              {data.dbg_serial_started === 1 &&
                data.dbg_suspended === 0 &&
                data.dbg_line_buf_len === 0 &&
                typeof data.dbg_uart_rx_avail === 'number' &&
                data.dbg_uart_rx_avail === 0 && (
                <Typography variant="body2" color="warning.main" sx={{ mt: 2, maxWidth: 560 }}>
                  UART shows no RX bytes (debug: <code>dbg_uart_rx_avail=0</code>, Serial1 started, not
                  suspended). For a direct scale link: connect <strong>scale TX → ESP GPIO18</strong> (UART
                  RX), <strong>GND</strong> common, match baud; use an RS-232↔TTL converter if the scale is
                  true RS-232 levels.
                </Typography>
              )}
            </>
          )}
        </Box>
      )}
    </SectionContent>
  );
};

export default SerialRest;
