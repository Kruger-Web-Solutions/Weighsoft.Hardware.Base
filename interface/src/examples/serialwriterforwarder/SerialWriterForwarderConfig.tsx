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
    <SectionContent title="Serial Writer Forwarder Configuration" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        The forwarder pulls lines from a remote HTTP or WebSocket source. Choose where each line is written:
        <strong> Native USB CDC</strong> (clean data on the ESP32-S3 <em>USB</em> port, no log mixing),
        <strong> Serial1 TX (GPIO17)</strong> for external scale hardware via a USB-TTL adapter, or
        <strong> Both</strong> (mirror). Developer logs always stay on the on-board <em>UART</em> port at 115200 baud.
        UART mode must be set to Writer when using Serial1.
      </Alert>

      <Box display="flex" flexDirection="column" gap={3}>
        <FormControlLabel
          control={
            <Checkbox
              checked={data.enabled}
              onChange={(e) => setField('enabled')(e.target.checked)}
            />
          }
          label="Enable Serial Writer Forwarder"
        />

        <FormControl fullWidth>
          <InputLabel>Forwarder output</InputLabel>
          <Select
            value={data.output_targets ?? ForwarderOutputTargets.UsbOnly}
            onChange={(e) => setField('output_targets')(e.target.value as ForwarderOutputTargets)}
            label="Forwarder output"
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
          label="Source URL"
          value={data.source_url ?? ''}
          onChange={(e) => setField('source_url')(e.target.value)}
          placeholder={
            data.protocol === ForwarderSourceProtocol.HTTP
              ? 'http://192.168.1.100/rest/serial'
              : 'ws://192.168.1.100/ws/serial'
          }
          helperText={
            data.protocol === ForwarderSourceProtocol.HTTP
              ? 'HTTP endpoint to GET (expects JSON response)'
              : 'WebSocket endpoint to subscribe to'
          }
        />

        <TextField
          fullWidth
          label="JSON Field"
          value={data.json_field ?? ''}
          onChange={(e) => setField('json_field')(e.target.value)}
          placeholder="last_line"
          helperText="Field name in the JSON response to use as the serial line (default: last_line)"
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
