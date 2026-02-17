import React, { FC, useEffect } from 'react';
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
  Alert,
  MenuItem,
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readSerialData, updateSerialData } from '../../api/serial';
import { SerialData } from '../../types/serial';
import { sanitizeLastLineForDisplay } from './formatSerialTimestamp';

const BAUDRATE_OPTIONS = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400];

const SerialConfig: FC = () => {
  const {
    data,
    loadData,
    saveData,
    saving,
    setData,
    errorMessage,
  } = useRest<SerialData>({ read: readSerialData, update: updateSerialData });

  const setField = (key: keyof SerialData) => (value: string | number) => {
    setData((prev) => (prev ? { ...prev, [key]: value } : prev));
  };

  if (!data) {
    return (
      <SectionContent title="Configuration" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const handleApply = () => {
    saveData();
  };

  // Keep "Current Data Preview" in sync with device (poll every 2s); only update last_line/weight/timestamp
  useEffect(() => {
    const interval = setInterval(() => {
      readSerialData()
        .then((res) => {
          setData((prev) =>
            prev
              ? {
                  ...prev,
                  last_line: res.data.last_line,
                  weight: res.data.weight,
                  timestamp: res.data.timestamp,
                }
              : prev
          );
        })
        .catch(() => {});
    }, 2000);
    return () => clearInterval(interval);
  }, [setData]);

  return (
    <SectionContent title="Serial Port Configuration" titleGutter>
      <Alert severity="warning" sx={{ mb: 2 }}>
        Changing serial configuration will restart the Serial2 connection. Ensure your scale or
        device uses the selected baud rate and format.
      </Alert>

      <Box component="form" sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
        <TextField
          select
          fullWidth
          label="Baud Rate"
          value={Number(data.baud_rate) || 115200}
          onChange={(e) => setField('baud_rate')(Number(e.target.value))}
          disabled={saving}
        >
          {BAUDRATE_OPTIONS.map((rate) => (
            <MenuItem key={rate} value={rate}>
              {rate}
            </MenuItem>
          ))}
        </TextField>

        <FormControl component="fieldset" disabled={saving}>
          <FormLabel component="legend">Data Bits</FormLabel>
          <RadioGroup
            row
            value={String(data.data_bits ?? 8)}
            onChange={(e) => setField('data_bits')(Number(e.target.value))}
          >
            {[5, 6, 7, 8].map((n) => (
              <FormControlLabel key={n} value={String(n)} control={<Radio />} label={String(n)} />
            ))}
          </RadioGroup>
        </FormControl>

        <FormControl component="fieldset" disabled={saving}>
          <FormLabel component="legend">Stop Bits</FormLabel>
          <RadioGroup
            row
            value={String(data.stop_bits ?? 1)}
            onChange={(e) => setField('stop_bits')(Number(e.target.value))}
          >
            <FormControlLabel value="1" control={<Radio />} label="1" />
            <FormControlLabel value="2" control={<Radio />} label="2" />
          </RadioGroup>
        </FormControl>

        <FormControl component="fieldset" disabled={saving}>
          <FormLabel component="legend">Parity</FormLabel>
          <RadioGroup
            row
            value={String(data.parity ?? 0)}
            onChange={(e) => setField('parity')(Number(e.target.value))}
          >
            <FormControlLabel value="0" control={<Radio />} label="None" />
            <FormControlLabel value="1" control={<Radio />} label="Even" />
            <FormControlLabel value="2" control={<Radio />} label="Odd" />
          </RadioGroup>
        </FormControl>

        <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
          Weight Extraction
        </Typography>
        <TextField
          fullWidth
          label="Regex Pattern"
          placeholder="e.g. (\d+\.\d+) or Weight: (\d+\.?\d*)"
          value={data.regex_pattern ?? ''}
          onChange={(e) => setField('regex_pattern')(e.target.value)}
          disabled={saving}
          helperText="First capture group is used as extracted weight. Leave empty to disable."
        />

        <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
          Current Data Preview
        </Typography>
        <Box sx={{ bgcolor: 'background.paper', p: 2, borderRadius: 1 }}>
          <Typography variant="caption" color="text.secondary">
            Last line:
          </Typography>
          <Typography sx={{ fontFamily: 'monospace', wordBreak: 'break-all' }}>
            {data.last_line
              ? sanitizeLastLineForDisplay(data.last_line)
              : '(none yet)'}
          </Typography>
          <Typography variant="caption" color="text.secondary" sx={{ mt: 1, display: 'block' }}>
            Extracted weight:
          </Typography>
          <Typography sx={{ fontFamily: 'monospace' }}>
            {data.weight !== undefined && data.weight !== '' ? data.weight : '(none)'}
          </Typography>
        </Box>

        <ButtonRow mt={2}>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving}
            variant="contained"
            color="primary"
            onClick={handleApply}
          >
            Apply Configuration
          </Button>
        </ButtonRow>
      </Box>
    </SectionContent>
  );
};

export default SerialConfig;
