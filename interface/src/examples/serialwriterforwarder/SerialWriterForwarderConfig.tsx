import { FC } from 'react';
import {
  Alert,
  Box,
  Button,
  Checkbox,
  FormControl,
  FormControlLabel,
  InputLabel,
  MenuItem,
  Select,
  TextField,
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

  const setField = (key: keyof SerialWriterForwarderData) => (value: any) => {
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
        The forwarder pulls lines from a remote HTTP or WebSocket source. Choose where each line is written: USB CDC
        (PC COM), Serial1 TX (GPIO17, for scale hardware), or both (mirror). UART mode must be Writer when using
        Serial1.
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
            <MenuItem value={ForwarderOutputTargets.UsbOnly}>USB CDC only (PC opens ESP USB COM)</MenuItem>
            <MenuItem value={ForwarderOutputTargets.Serial1Only}>Serial1 TX only (GPIO17)</MenuItem>
            <MenuItem value={ForwarderOutputTargets.Both}>USB CDC + Serial1 TX (mirror)</MenuItem>
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
          value={data.source_url}
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
          value={data.json_field}
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
          <TextField
            fullWidth
            size="small"
            label="Source login (password)"
            type="password"
            autoComplete="current-password"
            value={data.auth_password ?? ''}
            onChange={(e) => setField('auth_password')(e.target.value)}
            placeholder="Password for source device"
          />
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
