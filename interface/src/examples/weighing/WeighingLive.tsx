import React, { FC, useState } from 'react';
import {
  Typography,
  Box,
  Button,
  Alert,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogContentText,
  DialogActions,
  TextField,
} from '@mui/material';
import { SectionContent } from '../../components';
import { WEIGHING_SOCKET_PATH } from '../../api/endpoints';
import { useWs } from '../../utils';
import { updateWeighingData } from '../../api/weighing';
import { WeighingData } from '../../types/weighing';

const WeighingLive: FC = () => {
  const { data, updateData: _updateData } = useWs<WeighingData>(WEIGHING_SOCKET_PATH);

  const [presetDialogOpen, setPresetDialogOpen] = useState(false);
  const [presetValue, setPresetValue] = useState('');

  const sendCommand = async (command: Partial<WeighingData & {
    tare?: boolean;
    preset_tare?: boolean;
    preset_tare_value?: number;
    clear_tare?: boolean;
    zero?: boolean;
  }>) => {
    try {
      await updateWeighingData(command as any);
    } catch (_) {
      // WebSocket handles live updates, REST sends commands
    }
  };

  const handlePresetTare = async () => {
    const val = parseFloat(presetValue);
    if (!isNaN(val) && val >= 0) {
      await sendCommand({ preset_tare: true, preset_tare_value: val } as any);
    }
    setPresetDialogOpen(false);
    setPresetValue('');
  };

  if (!data) {
    return (
      <SectionContent title="Live Stream (WebSocket)" titleGutter>
        <Typography color="text.secondary">Connecting to WebSocket...</Typography>
      </SectionContent>
    );
  }

  const decPlaces = data.decimal_places ?? 2;

  return (
    <SectionContent title="Live Stream (WebSocket)" titleGutter>
      {/* Error states */}
      {!data.adc_connected && (
        <Alert severity="error" sx={{ mb: 2 }}>
          ADC not connected. Check ADS1231 wiring (DOUT=GPIO25, SCLK=GPIO23).
        </Alert>
      )}
      {data.overload && (
        <Alert severity="error" sx={{ mb: 2 }}>
          OVERLOAD — Remove weight exceeding max capacity ({data.max_capacity} {data.unit}).
        </Alert>
      )}
      {data.underload && (
        <Alert severity="error" sx={{ mb: 2 }}>
          UNDERLOAD — Check load cell connection.
        </Alert>
      )}

      {/* Main weight display */}
      <Box
        sx={{
          textAlign: 'center',
          py: 3,
          px: 2,
          bgcolor: 'background.paper',
          borderRadius: 2,
          mb: 2,
          border: '1px solid',
          borderColor: 'divider',
        }}
      >
        {/* Mode indicator */}
        <Typography variant="caption" color="text.secondary" sx={{ display: 'block', mb: 0.5 }}>
          {data.weight_mode === 'net' ? `NET  (Tare: ${data.tare_value?.toFixed(decPlaces)} ${data.unit})` : 'GROSS'}
        </Typography>

        {/* Weight value */}
        <Typography
          variant="h2"
          component="div"
          sx={{
            fontFamily: 'monospace',
            fontWeight: 'bold',
            color: (data.overload || data.underload) ? 'error.main' : 'text.primary',
          }}
        >
          {data.weight?.toFixed(decPlaces)}
        </Typography>
        <Typography variant="h5" color="text.secondary">{data.unit}</Typography>

        {/* Status indicators */}
        <Box sx={{ display: 'flex', justifyContent: 'center', gap: 2, mt: 1 }}>
          <Typography
            variant="body2"
            color={data.stable ? 'success.main' : 'warning.main'}
            sx={{ fontWeight: 'bold' }}
          >
            {data.stable ? '✓ STABLE' : '~ MOTION'}
          </Typography>
          {data.center_of_zero && (
            <Typography variant="body2" color="primary.main" sx={{ fontWeight: 'bold' }}>
              {'>'} 0 {'<'}
            </Typography>
          )}
        </Box>
      </Box>

      {/* Operation buttons */}
      <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 1, mb: 2 }}>
        <Button
          variant="contained"
          disabled={!data.stable}
          onClick={() => sendCommand({ tare: true } as any)}
        >
          Tare
        </Button>
        <Button
          variant="outlined"
          onClick={() => setPresetDialogOpen(true)}
        >
          Preset Tare
        </Button>
        <Button
          variant="outlined"
          disabled={data.tare_value === 0}
          onClick={() => sendCommand({ clear_tare: true } as any)}
        >
          Clear Tare
        </Button>
        <Button
          variant="outlined"
          color="secondary"
          disabled={!data.stable || data.seal_active}
          onClick={() => sendCommand({ zero: true } as any)}
        >
          Zero
        </Button>
      </Box>

      {/* Gross / Net / Tare summary */}
      <Box sx={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 1 }}>
        {[
          { label: 'Gross', value: data.gross_weight },
          { label: 'Net', value: data.net_weight },
          { label: 'Tare', value: data.tare_value },
        ].map(({ label, value }) => (
          <Box key={label} sx={{ textAlign: 'center', p: 1, bgcolor: 'background.paper', borderRadius: 1, border: '1px solid', borderColor: 'divider' }}>
            <Typography variant="caption" color="text.secondary">{label}</Typography>
            <Typography variant="body1" sx={{ fontFamily: 'monospace', fontWeight: 'bold' }}>
              {value?.toFixed(decPlaces)}
            </Typography>
          </Box>
        ))}
      </Box>

      {/* Preset Tare dialog */}
      <Dialog open={presetDialogOpen} onClose={() => setPresetDialogOpen(false)}>
        <DialogTitle>Preset Tare</DialogTitle>
        <DialogContent>
          <DialogContentText>
            Enter the known tare value (container weight).
          </DialogContentText>
          <TextField
            autoFocus
            margin="dense"
            label={`Tare Value (${data.unit})`}
            type="number"
            fullWidth
            value={presetValue}
            onChange={(e) => setPresetValue(e.target.value)}
            inputProps={{ step: 0.001, min: 0 }}
          />
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setPresetDialogOpen(false)}>Cancel</Button>
          <Button onClick={handlePresetTare} variant="contained">Apply</Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default WeighingLive;
