import React, { FC } from 'react';
import {
  Typography,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  Alert,
} from '@mui/material';
import { SectionContent } from '../../components';

const WeighingBle: FC = () => (
  <SectionContent title="BLE (Bluetooth Low Energy)" titleGutter>
    <Alert severity="info" sx={{ mb: 2 }}>
      When BLE is enabled, the Weighing Board exposes a GATT service. Subscribe to the
      characteristic to receive weight updates; write JSON to send commands.
    </Alert>

    <Typography variant="h6" gutterBottom>
      Service &amp; Characteristic UUIDs
    </Typography>
    <Table sx={{ mb: 3 }}>
      <TableHead>
        <TableRow>
          <TableCell><strong>Name</strong></TableCell>
          <TableCell><strong>UUID</strong></TableCell>
          <TableCell><strong>Properties</strong></TableCell>
        </TableRow>
      </TableHead>
      <TableBody>
        <TableRow>
          <TableCell>Weighing Service</TableCell>
          <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.8rem' }}>
            56780000-e8f2-537e-4f6c-d104768a5678
          </TableCell>
          <TableCell>—</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>Weighing Data</TableCell>
          <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.8rem' }}>
            56780001-e8f2-537e-4f6c-d104768a5678
          </TableCell>
          <TableCell>READ, WRITE, NOTIFY</TableCell>
        </TableRow>
      </TableBody>
    </Table>

    <Typography variant="h6" gutterBottom>
      Reading Weight (Notify)
    </Typography>
    <Typography variant="body2" paragraph>
      Subscribe to the Weighing Data characteristic to receive weight updates as JSON:
    </Typography>
    <Typography
      component="pre"
      variant="body2"
      sx={{ bgcolor: 'background.paper', p: 2, borderRadius: 1, overflowX: 'auto', mb: 2 }}
    >
{`{
  "weight": 12.34,
  "unit": "kg",
  "stable": true,
  "overload": false,
  "underload": false,
  "adc_connected": true
}`}
    </Typography>

    <Typography variant="h6" gutterBottom>
      Sending Commands (Write)
    </Typography>
    <Typography variant="body2" paragraph>
      Write JSON to the characteristic to send commands. Examples:
    </Typography>
    <Typography
      component="pre"
      variant="body2"
      sx={{ bgcolor: 'background.paper', p: 2, borderRadius: 1, overflowX: 'auto', mb: 2 }}
    >
{`// Tare
{ "tare": true }

// Preset tare
{ "preset_tare": true, "preset_tare_value": 0.5 }

// Clear tare
{ "clear_tare": true }

// Zero (requires instrument to be unsealed)
{ "zero": true }

// Calibrate (requires instrument to be unsealed)
{ "calibrate": true, "known_weight": 20.0 }

// Seal
{ "seal": true }

// Unseal (requires factory PIN)
{ "unseal": true, "pin": "123456" }`}
    </Typography>

    <Typography variant="h6" gutterBottom>
      BLE Settings
    </Typography>
    <Typography variant="body2" paragraph>
      BLE is enabled/disabled from the BLE Settings page (accessible from the system menu).
      Both the LED example service and Weighing service share the same BLE server and are
      configured when the BLE server starts.
    </Typography>
  </SectionContent>
);

export default WeighingBle;
