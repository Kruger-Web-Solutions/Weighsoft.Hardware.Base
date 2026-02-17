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
            <Typography color="text.secondary">No data received yet</Typography>
          )}
        </Box>
      )}
    </SectionContent>
  );
};

export default SerialRest;
