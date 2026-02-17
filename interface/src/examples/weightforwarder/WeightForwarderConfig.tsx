import { useState, useEffect } from 'react';
import {
  Box,
  Button,
  Checkbox,
  FormControl,
  FormControlLabel,
  InputLabel,
  MenuItem,
  Select,
  TextField,
  Typography,
} from '@mui/material';
import { ValidatedForm } from '../../components';
import { updateWeightForwarder, readWeightForwarder } from '../../api/weightForwarder';
import { ForwardProtocol, type WeightForwarderData } from '../../types/weightForwarder';
import { useSnackbar } from 'notistack';

// Feature flags from build configuration
const FT_MQTT = import.meta.env.VITE_FT_MQTT === '1';
const FT_BLE = import.meta.env.VITE_FT_BLE === '1';

export default function WeightForwarderConfig() {
  const { enqueueSnackbar } = useSnackbar();
  const [data, setData] = useState<WeightForwarderData>({
    protocol: ForwardProtocol.HTTP,
    target_url: '',
    ws_url: '',
    mqtt_topic: '',
    ble_service_uuid: '',
    ble_char_uuid: '',
    enabled: false,
    display_mode: false,
    connected: false,
    last_error: '',
    last_forward_time: 0,
  });

  const [loading, setLoading] = useState(true);

  useEffect(() => {
    readWeightForwarder()
      .then((config) => {
        setData(config);
        setLoading(false);
      })
      .catch((error) => {
        enqueueSnackbar('Failed to load configuration: ' + error.message, { variant: 'error' });
        setLoading(false);
      });
  }, [enqueueSnackbar]);

  const handleSubmit = () => {
    updateWeightForwarder(data)
      .then(() => {
        enqueueSnackbar('Configuration saved successfully', { variant: 'success' });
      })
      .catch((error) => {
        enqueueSnackbar('Failed to save configuration: ' + error.message, { variant: 'error' });
      });
  };

  const getAvailableProtocols = () => {
    const protocols = [
      { value: ForwardProtocol.HTTP, label: 'HTTP POST' },
      { value: ForwardProtocol.WS, label: 'WebSocket Client' },
    ];
    if (FT_MQTT) {
      protocols.push({ value: ForwardProtocol.MQTT, label: 'MQTT' });
    }
    if (FT_BLE) {
      protocols.push({ value: ForwardProtocol.BLE, label: 'BLE Client' });
    }
    return protocols;
  };

  if (loading) {
    return <Typography>Loading configuration...</Typography>;
  }

  return (
    <ValidatedForm onSubmit={handleSubmit}>
      <Box display="flex" flexDirection="column" gap={3}>
        <Typography variant="h6">Weight Stream Forwarder Configuration</Typography>

        <FormControlLabel
          control={
            <Checkbox
              checked={data.enabled}
              onChange={(e) => setData({ ...data, enabled: e.target.checked })}
            />
          }
          label="Enable Weight Forwarding"
        />

        <FormControl fullWidth>
          <InputLabel>Protocol</InputLabel>
          <Select
            value={data.protocol}
            onChange={(e) => setData({ ...data, protocol: e.target.value as ForwardProtocol })}
            label="Protocol"
          >
            {getAvailableProtocols().map((proto) => (
              <MenuItem key={proto.value} value={proto.value}>
                {proto.label}
              </MenuItem>
            ))}
          </Select>
        </FormControl>

        {data.protocol === ForwardProtocol.HTTP && (
          <TextField
            fullWidth
            label="Target URL"
            value={data.target_url}
            onChange={(e) => setData({ ...data, target_url: e.target.value })}
            placeholder="http://192.168.1.50:8080/weight"
            helperText="HTTP endpoint to POST weight data"
          />
        )}

        {data.protocol === ForwardProtocol.WS && (
          <TextField
            fullWidth
            label="WebSocket URL"
            value={data.ws_url}
            onChange={(e) => setData({ ...data, ws_url: e.target.value })}
            placeholder="ws://192.168.1.50:8080/ws"
            helperText="WebSocket endpoint for real-time streaming"
          />
        )}

        {data.protocol === ForwardProtocol.MQTT && FT_MQTT && (
          <TextField
            fullWidth
            label="MQTT Topic"
            value={data.mqtt_topic}
            onChange={(e) => setData({ ...data, mqtt_topic: e.target.value })}
            placeholder="remote/device/weight"
            helperText="MQTT topic to publish weight data"
          />
        )}

        {data.protocol === ForwardProtocol.BLE && FT_BLE && (
          <>
            <TextField
              fullWidth
              label="BLE Service UUID"
              value={data.ble_service_uuid}
              onChange={(e) => setData({ ...data, ble_service_uuid: e.target.value })}
              placeholder="12340000-e8f2-537e-4f6c-d104768a1234"
              helperText="Remote BLE service UUID"
            />
            <TextField
              fullWidth
              label="BLE Characteristic UUID"
              value={data.ble_char_uuid}
              onChange={(e) => setData({ ...data, ble_char_uuid: e.target.value })}
              placeholder="12340001-e8f2-537e-4f6c-d104768a1234"
              helperText="Remote BLE characteristic UUID for writing weight data"
            />
          </>
        )}

        <FormControlLabel
          control={
            <Checkbox
              checked={data.display_mode}
              onChange={(e) => setData({ ...data, display_mode: e.target.checked })}
            />
          }
          label="Display Mode (16-char line1/line2 format for LCD devices)"
        />

        <Box display="flex" justifyContent="flex-end">
          <Button type="submit" variant="contained" color="primary">
            Save Configuration
          </Button>
        </Box>
      </Box>
    </ValidatedForm>
  );
}
