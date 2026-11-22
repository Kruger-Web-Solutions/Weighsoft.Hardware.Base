import { FC, useEffect } from 'react';
import {
  TextField,
  Select,
  MenuItem,
  FormControl,
  InputLabel,
  Button,
  Alert,
  Box,
  Typography,
  Paper
} from '@mui/material';
import { SectionContent, FormLoader } from '../../components';
import { useRest } from '../../utils';
import { readLcdDisplayData, updateLcdDisplayData } from '../../api/lcddisplay';
import { LcdDisplayData } from '../../types/lcddisplay';

export const LcdDisplaySerialBridge: FC = () => {
  const { loadData, saveData, saving, data, setData } = useRest<LcdDisplayData>({
    read: readLcdDisplayData,
    update: updateLcdDisplayData
  });

  useEffect(() => {
    loadData();
  }, [loadData]);

  if (!data) {
    return <FormLoader />;
  }

  const handleModeChange = (mode: 'off' | 'websocket' | 'mqtt' | 'ble') => {
    setData({ ...data, bridge_mode: mode });
  };

  return (
    <SectionContent title="Serial Bridge">
      <Alert severity="info" sx={{ mb: 2 }}>
        Connect this Display to a Serial device to show streaming serial data on the LCD.
        Choose WebSocket for direct connection, MQTT for reliable pub/sub, or BLE for offline operation.
      </Alert>

      <FormControl fullWidth sx={{ mb: 2 }}>
        <InputLabel>Bridge Mode</InputLabel>
        <Select
          value={data.bridge_mode}
          label="Bridge Mode"
          onChange={(e) => handleModeChange(e.target.value as any)}
        >
          <MenuItem value="off">Off (Disabled)</MenuItem>
          <MenuItem value="websocket">WebSocket (Direct Connection)</MenuItem>
          <MenuItem value="mqtt">MQTT (Pub/Sub via Broker)</MenuItem>
          <MenuItem value="ble">BLE (Bluetooth - No WiFi)</MenuItem>
        </Select>
      </FormControl>

      {data.bridge_mode === 'websocket' && (
        <Paper elevation={1} sx={{ p: 2, mb: 2, bgcolor: 'background.paper' }}>
          <Typography variant="subtitle2" gutterBottom color="text.primary">
            WebSocket Configuration
          </Typography>
          <TextField
            fullWidth
            label="Serial Device IP"
            value={data.serial_device_ip}
            onChange={(e) => setData({ ...data, serial_device_ip: e.target.value })}
            helperText="e.g., 192.168.3.100"
            sx={{ mb: 2 }}
          />
          <TextField
            fullWidth
            type="number"
            label="Port"
            value={data.serial_device_port}
            onChange={(e) => setData({ ...data, serial_device_port: parseInt(e.target.value) || 80 })}
            helperText="Usually 80 for HTTP"
          />
        </Paper>
      )}

      {data.bridge_mode === 'mqtt' && (
        <Paper elevation={1} sx={{ p: 2, mb: 2, bgcolor: 'background.paper' }}>
          <Typography variant="subtitle2" gutterBottom color="text.primary">
            MQTT Configuration
          </Typography>
          <TextField
            fullWidth
            label="Serial MQTT Topic"
            value={data.serial_mqtt_topic}
            onChange={(e) => setData({ ...data, serial_mqtt_topic: e.target.value })}
            helperText="e.g., weighsoft/serial/a4e57cdb7928/data"
          />
          <Alert severity="info" sx={{ mt: 2 }}>
            Find the Serial device's MQTT topic in its web UI under the MQTT tab.
            Format: weighsoft/serial/[unique_id]/data
          </Alert>
        </Paper>
      )}

      {data.bridge_mode === 'ble' && (
        <Paper elevation={1} sx={{ p: 2, mb: 2, bgcolor: 'background.paper' }}>
          <Typography variant="subtitle2" gutterBottom color="text.primary">
            BLE Configuration
          </Typography>
          <TextField
            fullWidth
            label="Serial BLE Service UUID"
            value={data.serial_ble_service_uuid}
            onChange={(e) => setData({ ...data, serial_ble_service_uuid: e.target.value })}
            helperText="e.g., 0000181a-0000-1000-8000-00805f9b34fb"
            sx={{ mb: 2 }}
          />
          <TextField
            fullWidth
            label="Serial BLE Characteristic UUID"
            value={data.serial_ble_char_uuid}
            onChange={(e) => setData({ ...data, serial_ble_char_uuid: e.target.value })}
            helperText="e.g., 00002a58-0000-1000-8000-00805f9b34fb"
          />
          <Alert severity="info" sx={{ mt: 2 }}>
            Find the Serial device's BLE UUIDs in its web UI under the BLE tab.
            The Display will scan for and connect to the Serial device automatically.
          </Alert>
        </Paper>
      )}

      <Box sx={{ mt: 3 }}>
        <Button
          variant="contained"
          color="primary"
          onClick={saveData}
          disabled={saving || data.bridge_mode === 'off'}
        >
          {data.bridge_mode === 'off' ? 'Select Mode to Enable' : 'Apply Configuration'}
        </Button>
      </Box>

      <Paper elevation={1} sx={{ p: 2, mt: 3, bgcolor: 'background.paper' }}>
        <Typography variant="subtitle2" gutterBottom color="text.primary">
          Mode Comparison
        </Typography>
        <Typography variant="body2" color="text.secondary">
          <strong>WebSocket:</strong> Low latency, direct device-to-device. Requires IP address. Best for single display.
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
          <strong>MQTT:</strong> Reliable, broker-managed. No IP needed. Best for multiple displays or unstable networks.
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
          <strong>BLE:</strong> Works without WiFi. Automatic device discovery. Best for battery-powered or offline setups.
        </Typography>
      </Paper>
    </SectionContent>
  );
};
