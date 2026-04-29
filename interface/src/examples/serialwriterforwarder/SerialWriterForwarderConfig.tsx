import { FC } from 'react';
import {
  Alert,
  Box,
  Button,
  Checkbox,
  Chip,
  FormControl,
  FormControlLabel,
  InputLabel,
  MenuItem,
  Select,
  Stack,
  TextField,
  Typography,
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriterForwarder, updateSerialWriterForwarder } from '../../api/serialWriterForwarder';
import {
  ForwarderOutputTargets,
  ForwarderSourceProtocol,
  type SerialWriterForwarderData,
} from '../../types/serialWriterForwarder';

const SerialWriterForwarderConfig: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } =
    useRest<SerialWriterForwarderData>({
      read: readSerialWriterForwarder,
      update: updateSerialWriterForwarder,
    });

  const setField = (key: keyof SerialWriterForwarderData) => (value: string | number | boolean) => {
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
    <SectionContent title="Serial Reader Source Configuration" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Connect to a remote <strong>serialReader</strong> device and pull its weight / string data onto this
        device's outputs. Point the Source URL at your serialReader's WebSocket (<code>/ws/serial</code>) or
        HTTP (<code>/rest/serial</code>) endpoint and set JSON Field to <code>last_line</code>. Output
        options: <strong>Native USB CDC</strong> (clean data stream on the ESP32-S3 USB port),
        <strong> Serial1 TX (GPIO17)</strong> via USB-TTL adapter, or <strong>Both</strong>. UART mode must
        be <strong>Writer</strong> when using Serial1.
      </Alert>

      <Box display="flex" flexDirection="column" gap={3}>
        <FormControlLabel
          control={
            <Checkbox
              checked={data.enabled}
              onChange={(e) => setField('enabled')(e.target.checked)}
            />
          }
          label="Enable Serial Reader Source"
        />

        <FormControl fullWidth>
          <InputLabel>Output destination</InputLabel>
          <Select
            value={data.output_targets ?? ForwarderOutputTargets.UsbOnly}
            onChange={(e) => setField('output_targets')(e.target.value as ForwarderOutputTargets)}
            label="Output destination"
          >
            <MenuItem value={ForwarderOutputTargets.UsbOnly}>
              Native USB CDC only (PC opens ESP32-S3 USB-OTG COM, clean data, no log mixing)
            </MenuItem>
            <MenuItem value={ForwarderOutputTargets.Serial1Only}>Serial1 TX only (GPIO17, requires USB-TTL adapter)</MenuItem>
            <MenuItem value={ForwarderOutputTargets.Both}>Native USB CDC + Serial1 TX (mirror)</MenuItem>
          </Select>
        </FormControl>

        <FormControl fullWidth>
          <InputLabel>Source Protocol</InputLabel>
          <Select
            value={data.protocol}
            onChange={(e) => setField('protocol')(e.target.value as ForwarderSourceProtocol)}
            label="Source Protocol"
          >
            <MenuItem value={ForwarderSourceProtocol.HTTP}>HTTP Poll (GET request)</MenuItem>
            <MenuItem value={ForwarderSourceProtocol.WS}>WebSocket Subscribe</MenuItem>
          </Select>
        </FormControl>

        <TextField
          fullWidth
          label="serialReader device URL"
          value={data.source_url ?? ''}
          onChange={(e) => setField('source_url')(e.target.value)}
          placeholder={
            data.protocol === ForwarderSourceProtocol.HTTP
              ? 'http://192.168.1.xx/rest/serial'
              : 'ws://192.168.1.xx/ws/serial'
          }
          helperText={
            data.protocol === ForwarderSourceProtocol.HTTP
              ? 'Your serialReader device IP — HTTP endpoint e.g. http://192.168.1.xx/rest/serial'
              : 'Your serialReader device IP — WebSocket endpoint e.g. ws://192.168.1.xx/ws/serial'
          }
        />

        <TextField
          fullWidth
          label="JSON field to extract"
          value={data.json_field ?? ''}
          onChange={(e) => setField('json_field')(e.target.value)}
          placeholder="last_line"
          helperText='Field name from the serialReader JSON payload. Use "last_line" for the default serialReader output.'
        />

        {data.protocol === ForwarderSourceProtocol.HTTP && (
          <TextField
            fullWidth
            label="Poll Interval (ms)"
            type="number"
            value={data.poll_interval_ms}
            onChange={(e) => setField('poll_interval_ms')(Number(e.target.value))}
            inputProps={{ min: 100, step: 100 }}
            helperText="Minimum 100ms. How often to poll the HTTP source."
          />
        )}

        <Box>
          <TextField
            fullWidth
            size="small"
            label="Source login (username)"
            type="text"
            autoComplete="username"
            value={data.auth_username ?? ''}
            onChange={(e) => setField('auth_username')(e.target.value)}
            placeholder="Leave empty if source does not require login"
            helperText="Used for JWT Bearer token authentication against the source device"
            sx={{ mb: 1 }}
          />
          <Stack direction="row" alignItems="center" spacing={1} sx={{ mb: 0.5 }}>
            <Typography variant="caption" color="text.secondary">
              Source login (password)
            </Typography>
            {data.auth_password_set ? (
              <Chip label="stored" color="success" size="small" variant="outlined" />
            ) : (
              <Chip label="not set" color="default" size="small" variant="outlined" />
            )}
          </Stack>
          <TextField
            fullWidth
            size="small"
            label="Source login (password)"
            type="password"
            autoComplete="current-password"
            value={data.auth_password ?? ''}
            onChange={(e) => setField('auth_password')(e.target.value)}
            placeholder={
              data.auth_password_set
                ? 'Leave blank to keep stored password'
                : 'Password for source device'
            }
            helperText={
              data.auth_password_set
                ? 'A password is stored on the device. Leave this field blank on Save to keep it; type a new value to replace it.'
                : 'Type the source device password. The value is stored on flash and never returned by GET.'
            }
          />
          {data.auth_password_set && (
            <Button
              size="small"
              color="warning"
              sx={{ mt: 1 }}
              onClick={() => {
                setField('auth_password')('');
                setField('auth_password_clear')(true);
              }}
            >
              Clear stored password
            </Button>
          )}
        </Box>

        <ButtonRow>
          <Button
            startIcon={<SaveIcon />}
            variant="contained"
            color="primary"
            onClick={saveData}
            disabled={saving}
          >
            Save Configuration
          </Button>
        </ButtonRow>
      </Box>
    </SectionContent>
  );
};

export default SerialWriterForwarderConfig;
