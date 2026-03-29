import React, { FC } from 'react';
import {
  Typography,
  Box,
  Button,
  TextField,
  FormControl,
  FormLabel,
  RadioGroup,
  FormControlLabel,
  Radio,
  Checkbox,
  Alert,
  MenuItem,
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import LockIcon from '@mui/icons-material/Lock';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readWeighingData, updateWeighingData } from '../../api/weighing';
import { WeighingData } from '../../types/weighing';

const UNIT_OPTIONS = ['kg', 'g', 'lb'];

const WeighingConfig: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } =
    useRest<WeighingData>({ read: readWeighingData, update: updateWeighingData });

  if (!data) {
    return (
      <SectionContent title="Configuration" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const setField = <K extends keyof WeighingData>(key: K) =>
    (value: WeighingData[K]) => setData((prev) => (prev ? { ...prev, [key]: value } : prev));

  const sealed = data.seal_active;

  return (
    <SectionContent title="Scale Configuration" titleGutter>
      {sealed && (
        <Alert severity="warning" icon={<LockIcon />} sx={{ mb: 2 }}>
          Instrument is sealed (event counter: {data.event_counter}). Configuration changes are
          not permitted. Go to the Calibration tab to unseal.
        </Alert>
      )}
      {!sealed && (
        <Alert severity="info" sx={{ mb: 2 }}>
          Configure scale parameters. All changes are legally relevant and will require resealing.
        </Alert>
      )}

      <Box component="form" sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
        <TextField
          select
          fullWidth
          label="Unit"
          value={data.unit}
          onChange={(e) => setField('unit')(e.target.value as WeighingData['unit'])}
          disabled={saving || sealed}
        >
          {UNIT_OPTIONS.map((u) => (
            <MenuItem key={u} value={u}>{u}</MenuItem>
          ))}
        </TextField>

        <TextField
          fullWidth
          type="number"
          label="Max Capacity"
          value={data.max_capacity}
          onChange={(e) => setField('max_capacity')(parseFloat(e.target.value) || 0)}
          disabled={saving || sealed}
          helperText={`Maximum weighing capacity in ${data.unit}`}
          inputProps={{ step: 0.1, min: 0.1 }}
        />

        <TextField
          fullWidth
          type="number"
          label="Min Division (d)"
          value={data.min_division}
          onChange={(e) => setField('min_division')(parseFloat(e.target.value) || 0)}
          disabled={saving || sealed}
          helperText="Smallest readable division (e.g. 0.02 kg). Determines overload and stability thresholds."
          inputProps={{ step: 0.001, min: 0.001 }}
        />

        <TextField
          fullWidth
          type="number"
          label="Decimal Places"
          value={data.decimal_places}
          onChange={(e) => setField('decimal_places')(parseInt(e.target.value, 10) || 0)}
          disabled={saving || sealed}
          helperText="Number of decimal places for display (0–6)"
          inputProps={{ step: 1, min: 0, max: 6 }}
        />

        <FormControlLabel
          control={
            <Checkbox
              checked={data.zero_tracking_enabled}
              onChange={(e) => setField('zero_tracking_enabled')(e.target.checked)}
              disabled={saving || sealed}
            />
          }
          label="Enable Automatic Zero Tracking (AZT)"
        />

        {data.zero_tracking_enabled && (
          <TextField
            fullWidth
            type="number"
            label="Zero Tracking Range"
            value={data.zero_tracking_range}
            onChange={(e) => setField('zero_tracking_range')(parseFloat(e.target.value) || 0)}
            disabled={saving || sealed}
            helperText={`Max AZT correction per interval in ${data.unit}. Typically 0.5d.`}
            inputProps={{ step: 0.001, min: 0.001 }}
          />
        )}

        <TextField
          fullWidth
          type="number"
          label="Power-On Zero Range (%)"
          value={data.power_on_zero_range}
          onChange={(e) => setField('power_on_zero_range')(parseFloat(e.target.value) || 0)}
          disabled={saving || sealed}
          helperText="Auto-zero at startup if gross weight is within this % of max capacity (default 10%)"
          inputProps={{ step: 1, min: 1, max: 20 }}
        />

        <TextField
          fullWidth
          type="number"
          label="Manual Zero Range (%)"
          value={data.manual_zero_range}
          onChange={(e) => setField('manual_zero_range')(parseFloat(e.target.value) || 0)}
          disabled={saving || sealed}
          helperText="Max gross weight to allow manual zero (default 2% of capacity)"
          inputProps={{ step: 0.5, min: 0.5, max: 10 }}
        />

        <Typography variant="h6" gutterBottom sx={{ mt: 1 }}>
          ADC Settings
        </Typography>

        <FormControl component="fieldset" disabled={saving}>
          <FormLabel component="legend">Output Rate</FormLabel>
          <RadioGroup
            row
            value={String(data.speed_mode ?? 0)}
            onChange={(e) => setField('speed_mode')(parseInt(e.target.value, 10))}
          >
            <FormControlLabel value="0" control={<Radio />} label="10 SPS (low noise)" />
            <FormControlLabel value="1" control={<Radio />} label="80 SPS (fast)" />
          </RadioGroup>
        </FormControl>

        <TextField
          fullWidth
          type="number"
          label="Samples to Average"
          value={data.samples_to_average}
          onChange={(e) => setField('samples_to_average')(parseInt(e.target.value, 10) || 1)}
          disabled={saving}
          helperText="Number of ADC readings to average per update (1–32)"
          inputProps={{ step: 1, min: 1, max: 32 }}
        />

        <ButtonRow mt={2}>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving || sealed}
            variant="contained"
            color="primary"
            onClick={saveData}
          >
            Apply Configuration
          </Button>
        </ButtonRow>
      </Box>
    </SectionContent>
  );
};

export default WeighingConfig;
