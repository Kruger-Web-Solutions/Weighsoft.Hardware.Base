import { FC } from 'react';

import BluetoothIcon from '@mui/icons-material/Bluetooth';
import {
  Alert,
  Box,
  Chip,
  Divider,
  List,
  ListItem,
  ListItemText,
  Paper,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography
} from '@mui/material';

import { SectionContent } from '../../components';

interface CharacteristicInfo {
  name: string;
  uuid: string;
  properties: string;
  description: string;
  fields: { key: string; type: string; description: string; writable?: boolean }[];
}

const GROWBOX_BLE_SERVICE_UUID = '4b6e5d7f-bf21-4e9a-9c87-6e5d1042a3b1';

const CHARACTERISTICS: CharacteristicInfo[] = [
  {
    name: 'Controls',
    uuid: 'a3b4c5d6-bf21-4e9a-9c87-6e5d1042a3b2',
    properties: 'Read / Write / Notify',
    description: 'Relay outputs and moisture alarm state. Write JSON to control relays or acknowledge the alarm.',
    fields: [
      { key: 'la', type: 'boolean', description: 'Light A on/off', writable: true },
      { key: 'lb', type: 'boolean', description: 'Light B on/off', writable: true },
      { key: 'fi', type: 'boolean', description: 'Fan In on/off', writable: true },
      { key: 'fo', type: 'boolean', description: 'Fan Out on/off', writable: true },
      { key: 'ma', type: 'boolean', description: 'Moisture alarm active (read-only)' },
      { key: 'mak', type: 'boolean', description: 'Moisture alarm acknowledged', writable: true }
    ]
  },
  {
    name: 'Sensors',
    uuid: 'b5c6d7e8-bf21-4e9a-9c87-6e5d1042a3b3',
    properties: 'Read / Notify',
    description:
      'Live sensor readings. Subscribe to notifications to receive updates on every sensor poll (default 5 s).',
    fields: [
      { key: 'it', type: 'float', description: 'Inside temperature (°C)' },
      { key: 'ih', type: 'float', description: 'Inside humidity (%)' },
      { key: 'ot', type: 'float', description: 'Outside temperature (°C)' },
      { key: 'oh', type: 'float', description: 'Outside humidity (%)' },
      { key: 'mp', type: 'float', description: 'Soil moisture (%)' },
      { key: 'isf', type: 'boolean', description: 'Inside sensor fault flag' },
      { key: 'osf', type: 'boolean', description: 'Outside sensor fault flag' },
      { key: 'msf', type: 'boolean', description: 'Moisture sensor fault flag' },
      { key: 'ntp', type: 'boolean', description: 'NTP sync OK' },
      { key: 'wifi', type: 'boolean', description: 'WiFi connected' }
    ]
  },
  {
    name: 'Automation',
    uuid: 'c7d8e9f0-bf21-4e9a-9c87-6e5d1042a3b4',
    properties: 'Read / Write / Notify',
    description: 'Light schedule, fan thresholds, and moisture alarm settings. Write full or partial JSON to update.',
    fields: [
      { key: 'light_schedule_enabled', type: 'boolean', description: 'Enable daily light schedule', writable: true },
      { key: 'light_on_time', type: 'string', description: 'Lights on time "HH:MM" (local time)', writable: true },
      { key: 'light_off_time', type: 'string', description: 'Lights off time "HH:MM" (local time)', writable: true },
      { key: 'fan_control_enabled', type: 'boolean', description: 'Enable temperature-driven fans', writable: true },
      {
        key: 'fan_temperature_threshold',
        type: 'float',
        description: 'Fan on temperature threshold (°C)',
        writable: true
      },
      { key: 'fan_hysteresis', type: 'float', description: 'Fan hysteresis band (°C)', writable: true },
      { key: 'moisture_alarm_enabled', type: 'boolean', description: 'Enable moisture alarm', writable: true },
      {
        key: 'moisture_alarm_threshold',
        type: 'int',
        description: 'Alarm fires below this moisture % ',
        writable: true
      },
      {
        key: 'moisture_alarm_dwell_ms',
        type: 'uint32',
        description: 'Dwell time before alarm triggers (ms)',
        writable: true
      }
    ]
  }
];

const GrowboxBleInfo: FC = () => (
  <SectionContent title="Bluetooth Low Energy Interface">
    <Alert severity="info" icon={<BluetoothIcon />} sx={{ mb: 2 }}>
      BLE must be enabled in <strong>BLE Settings</strong> before the Growbox BLE service starts. Use a generic BLE
      scanner (e.g. nRF Connect, LightBlue) or your own mobile app to connect.
    </Alert>

    <Typography variant="h6" gutterBottom>
      Service UUID
    </Typography>
    <Paper variant="outlined" sx={{ p: 2, mb: 3, fontFamily: 'monospace', fontSize: '0.9rem', wordBreak: 'break-all' }}>
      {GROWBOX_BLE_SERVICE_UUID}
    </Paper>

    <Divider sx={{ mb: 3 }} />

    {CHARACTERISTICS.map((char) => (
      <Box key={char.uuid} sx={{ mb: 4 }}>
        <Stack direction="row" alignItems="center" spacing={1} sx={{ mb: 1 }}>
          <BluetoothIcon fontSize="small" color="primary" />
          <Typography variant="subtitle1" fontWeight="bold">
            {char.name} Characteristic
          </Typography>
          <Chip label={char.properties} size="small" variant="outlined" />
        </Stack>

        <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
          {char.description}
        </Typography>

        <Typography variant="caption" color="text.disabled" sx={{ fontFamily: 'monospace', display: 'block', mb: 1 }}>
          {char.uuid}
        </Typography>

        <TableContainer component={Paper} variant="outlined">
          <Table size="small">
            <TableHead>
              <TableRow>
                <TableCell>
                  <strong>Key</strong>
                </TableCell>
                <TableCell>
                  <strong>Type</strong>
                </TableCell>
                <TableCell>
                  <strong>Description</strong>
                </TableCell>
                <TableCell align="center">
                  <strong>Writable</strong>
                </TableCell>
              </TableRow>
            </TableHead>
            <TableBody>
              {char.fields.map((field) => (
                <TableRow key={field.key}>
                  <TableCell sx={{ fontFamily: 'monospace' }}>{field.key}</TableCell>
                  <TableCell sx={{ fontFamily: 'monospace', color: 'text.secondary' }}>{field.type}</TableCell>
                  <TableCell>{field.description}</TableCell>
                  <TableCell align="center">{field.writable ? '✓' : '—'}</TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </TableContainer>
      </Box>
    ))}

    <Divider sx={{ mb: 3 }} />

    <Typography variant="h6" gutterBottom>
      Usage Examples
    </Typography>

    <Typography variant="subtitle2" gutterBottom>
      Turn lights on
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write to Controls: {"la":true,"lb":true}`}
    </Paper>

    <Typography variant="subtitle2" gutterBottom>
      Turn Fan In on
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write to Controls: {"fi":true}`}
    </Paper>

    <Typography variant="subtitle2" gutterBottom>
      Acknowledge moisture alarm
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write to Controls: {"mak":true}`}
    </Paper>

    <Typography variant="subtitle2" gutterBottom>
      Update fan threshold via BLE
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write to Automation: {"fan_control_enabled":true,"fan_temperature_threshold":28.0}`}
    </Paper>

    <Typography variant="subtitle2" gutterBottom>
      All relay states are preserved across power cycles — automation-driven states resume automatically on boot.
    </Typography>

    <Alert severity="warning" sx={{ mt: 2 }}>
      <List dense disablePadding>
        <ListItem disablePadding>
          <ListItemText primary="Abbreviated keys (la, lb, fi, fo…) are used on the BLE interface to keep payloads within BLE MTU limits." />
        </ListItem>
        <ListItem disablePadding>
          <ListItemText primary="The Automation characteristic uses full key names (same as REST/WebSocket API)." />
        </ListItem>
        <ListItem disablePadding>
          <ListItemText primary="Values received over BLE go through the same automation evaluation — manual overrides may be overwritten by the next automation tick." />
        </ListItem>
      </List>
    </Alert>
  </SectionContent>
);

export default GrowboxBleInfo;
