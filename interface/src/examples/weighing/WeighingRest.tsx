import React, { FC, useEffect } from 'react';
import { Typography, Box } from '@mui/material';
import { SectionContent, FormLoader } from '../../components';
import { useRest } from '../../utils';
import { readWeighingData } from '../../api/weighing';
import { WeighingData } from '../../types/weighing';

const WeighingRest: FC = () => {
  const { data, loadData, errorMessage } = useRest<WeighingData>({ read: readWeighingData });

  // Poll every 2 seconds (same pattern as SerialRest)
  useEffect(() => {
    const interval = setInterval(() => {
      loadData();
    }, 2000);
    return () => clearInterval(interval);
  }, [loadData]);

  return (
    <SectionContent title="REST View (Current Reading)" titleGutter>
      <Typography variant="body2" paragraph>
        This view polls the REST API every 2 seconds to fetch the current reading.
        For real-time streaming, use the Live Stream tab.
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
            minHeight: 120,
          }}
        >
          <Typography variant="caption" color="text.secondary">Weight:</Typography>
          <Typography sx={{ fontFamily: 'monospace', fontSize: '1.4rem', fontWeight: 'bold' }}>
            {data.weight?.toFixed(data.decimal_places ?? 2)} {data.unit}
          </Typography>

          <Box sx={{ mt: 1.5, display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 0.5 }}>
            <Typography variant="body2" color="text.secondary">
              Gross: <strong>{data.gross_weight?.toFixed(data.decimal_places ?? 2)}</strong>
            </Typography>
            <Typography variant="body2" color="text.secondary">
              Net: <strong>{data.net_weight?.toFixed(data.decimal_places ?? 2)}</strong>
            </Typography>
            <Typography variant="body2" color="text.secondary">
              Tare: <strong>{data.tare_value?.toFixed(data.decimal_places ?? 2)}</strong>
            </Typography>
            <Typography variant="body2" color="text.secondary">
              Mode: <strong>{data.weight_mode}</strong>
            </Typography>
            <Typography variant="body2" color={data.stable ? 'success.main' : 'warning.main'}>
              Stable: <strong>{data.stable ? 'Yes' : 'No'}</strong>
            </Typography>
            <Typography variant="body2" color={data.adc_connected ? 'success.main' : 'error.main'}>
              ADC: <strong>{data.adc_connected ? 'Connected' : 'ERROR'}</strong>
            </Typography>
            {data.overload && (
              <Typography variant="body2" color="error.main">
                <strong>OVERLOAD</strong>
              </Typography>
            )}
            {data.underload && (
              <Typography variant="body2" color="error.main">
                <strong>UNDERLOAD</strong>
              </Typography>
            )}
          </Box>

          <Box sx={{ mt: 1.5 }}>
            <Typography variant="caption" color="text.secondary">
              Raw ADC: {data.raw_value} &nbsp;|&nbsp; Event counter: {data.event_counter}
              {data.seal_active && ' | 🔒 SEALED'}
            </Typography>
          </Box>
        </Box>
      )}
    </SectionContent>
  );
};

export default WeighingRest;
