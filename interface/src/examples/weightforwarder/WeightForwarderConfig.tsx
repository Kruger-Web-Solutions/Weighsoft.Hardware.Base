import { FC, useContext } from 'react';
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
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { FeaturesContext } from '../../contexts/features';
import { useRest } from '../../utils';
import { updateWeightForwarder, readWeightForwarder } from '../../api/weightForwarder';
import { ForwardProtocol, type WeightForwarderData } from '../../types/weightForwarder';

const WeightForwarderConfig: FC = () => {
  const { features } = useContext(FeaturesContext);
  const {
    data,
    loadData,
    saveData,
    saving,
    setData,
    errorMessage,
  } = useRest<WeightForwarderData>({ read: readWeightForwarder, update: updateWeightForwarder });

  const setField = (key: keyof WeightForwarderData) => (value: any) => {
    setData((prev) => (prev ? { ...prev, [key]: value } : prev));
  };

  if (!data) {
    return (
      <SectionContent title="Configuration" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const getAvailableProtocols = () => {
    const protocols = [
      { value: ForwardProtocol.HTTP, label: 'HTTP POST' },
      { value: ForwardProtocol.WS, label: 'WebSocket Client' },
    ];
    if (features.mqtt) {
      protocols.push({ value: ForwardProtocol.MQTT, label: 'MQTT' });
    }
    if (features.ble) {
      protocols.push({ value: ForwardProtocol.BLE, label: 'BLE Client' });
    }
    return protocols;
  };

  return (
    <SectionContent title="Weight Stream Forwarder Configuration" titleGutter>
      <Box display="flex" flexDirection="column" gap={3}>
        <FormControlLabel
          control={
            <Checkbox
              checked={data.enabled}
              onChange={(e) => setField('enabled')(e.target.checked)}
            />
          }
          label="Enable Weight Forwarding"
        />

        <FormControl fullWidth>
          <InputLabel>Protocol</InputLabel>
          <Select
            value={data.protocol}
            onChange={(e) => setField('protocol')(e.target.value as ForwardProtocol)}
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
          <>
            <TextField
              fullWidth
              label="Target URL"
              value={data.target_url}
              onChange={(e) => setField('target_url')(e.target.value)}
              placeholder="http://192.168.1.50:8080/weight or http://192.168.3.100/rest/display"
              helperText="HTTP endpoint to POST weight data"
            />
            <Box sx={{ mt: 1 }}>
              <TextField
                fullWidth
                size="small"
                label="Target login (username)"
                type="text"
                autoComplete="username"
                value={data.auth_username ?? ''}
                onChange={(e) => setField('auth_username')(e.target.value)}
                placeholder="Leave empty if target does not require login"
                helperText="Required when target requires login (e.g. display device returns HTTP 401)"
                sx={{ mb: 1 }}
              />
              <TextField
                fullWidth
                size="small"
                label="Target login (password)"
                type="password"
                autoComplete="current-password"
                value={data.auth_password ?? ''}
                onChange={(e) => setField('auth_password')(e.target.value)}
                placeholder="Password for target device"
              />
            </Box>
          </>
        )}

        {data.protocol === ForwardProtocol.WS && (
          <>
          <TextField
            fullWidth
            label="WebSocket URL"
            value={data.ws_url}
            onChange={(e) => setField('ws_url')(e.target.value)}
            placeholder="ws://192.168.3.100/ws/display"
            helperText="WebSocket endpoint for real-time streaming"
          />
          <Box sx={{ mt: 1 }}>
            <TextField
              fullWidth
              size="small"
              label="Target login (username)"
              type="text"
              autoComplete="username"
              value={data.auth_username ?? ''}
              onChange={(e) => setField('auth_username')(e.target.value)}
              placeholder="Leave empty if target does not require login"
              helperText="Required when target requires login (token appended as ?access_token=)"
              sx={{ mb: 1 }}
            />
            <TextField
              fullWidth
              size="small"
              label="Target login (password)"
              type="password"
              autoComplete="current-password"
              value={data.auth_password ?? ''}
              onChange={(e) => setField('auth_password')(e.target.value)}
              placeholder="Password for target device"
            />
          </Box>
          </>
        )}

        {data.protocol === ForwardProtocol.MQTT && features.mqtt && (
          <TextField
            fullWidth
            label="MQTT Topic"
            value={data.mqtt_topic}
            onChange={(e) => setField('mqtt_topic')(e.target.value)}
            placeholder="remote/device/weight"
            helperText="MQTT topic to publish weight data"
          />
        )}

        {data.protocol === ForwardProtocol.BLE && features.ble && (
          <>
            <TextField
              fullWidth
              label="BLE Service UUID"
              value={data.ble_service_uuid}
              onChange={(e) => setField('ble_service_uuid')(e.target.value)}
              placeholder="12340000-e8f2-537e-4f6c-d104768a1234"
              helperText="Remote BLE service UUID"
            />
            <TextField
              fullWidth
              label="BLE Characteristic UUID"
              value={data.ble_char_uuid}
              onChange={(e) => setField('ble_char_uuid')(e.target.value)}
              placeholder="12340001-e8f2-537e-4f6c-d104768a1234"
              helperText="Remote BLE characteristic UUID for writing weight data"
            />
          </>
        )}

        <FormControlLabel
          control={
            <Checkbox
              checked={data.display_mode}
              onChange={(e) => setField('display_mode')(e.target.checked)}
            />
          }
          label="Display Mode (16-char line1/line2 format for LCD devices)"
        />

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

export default WeightForwarderConfig;
