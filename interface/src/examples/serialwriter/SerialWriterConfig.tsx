import { FC } from 'react';
import {
  Alert,
  Box,
  Button,
  FormControl,
  FormControlLabel,
  FormLabel,
  MenuItem,
  Radio,
  RadioGroup,
  TextField,
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriterConfig } from '../../api/serialWriter';
import type { SerialWriterData, SerialWriterConfigUpdate, LineTerminator } from '../../types/serialWriter';

const BAUDRATE_OPTIONS = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400];

const LINE_TERMINATOR_OPTIONS: { value: LineTerminator; label: string }[] = [
  { value: 'LF', label: 'LF (\\n) — Unix/Linux' },
  { value: 'CRLF', label: 'CRLF (\\r\\n) — Windows/Modems' },
  { value: 'CR', label: 'CR (\\r) — Legacy' },
  { value: 'NONE', label: 'None — raw bytes only' },
];

const SerialWriterConfig: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({
    read: readSerialWriter,
    update: (d) => updateSerialWriterConfig(d as SerialWriterConfigUpdate),
  });

  const setField = (key: keyof SerialWriterData) => (value: string | number) => {
    setData((prev) => (prev ? { ...prev, [key]: value } : prev));
  };

  if (!data) {
    return (
      <SectionContent title="Configuration" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Serial Writer Configuration" titleGutter>
      <Alert severity="warning" sx={{ mb: 2 }}>
        Changing serial configuration restarts the Serial1 connection. Make sure UART Mode is set to{' '}
        <strong>Serial Writer</strong> before using this interface.
      </Alert>

      <Box component="form" sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
        <TextField
          select
          fullWidth
          label="Baud Rate"
          value={Number(data.baud_rate) || 9600}
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

        <TextField
          select
          fullWidth
          label="Line Terminator"
          value={data.line_terminator ?? 'LF'}
          onChange={(e) => setField('line_terminator')(e.target.value)}
          disabled={saving}
          helperText="Appended automatically after each line sent"
        >
          {LINE_TERMINATOR_OPTIONS.map((opt) => (
            <MenuItem key={opt.value} value={opt.value}>
              {opt.label}
            </MenuItem>
          ))}
        </TextField>

        <ButtonRow mt={2}>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving}
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

export default SerialWriterConfig;
