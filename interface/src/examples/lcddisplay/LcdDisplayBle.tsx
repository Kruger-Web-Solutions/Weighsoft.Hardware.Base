import React, { FC } from 'react';
import { Typography, Paper, Alert, List, ListItem, ListItemText } from '@mui/material';

import { SectionContent } from '../../components';

const LcdDisplayBle: FC = () => (
  <SectionContent title="BLE Control" titleGutter>
    <Alert severity="info" sx={{ mb: 2 }}>
      Control the LCD display via Bluetooth Low Energy (BLE).
    </Alert>

    <Typography variant="h6" gutterBottom>
      BLE Configuration
    </Typography>
    <Paper elevation={1} sx={{ p: 2, bgcolor: 'background.paper', mb: 2 }}>
      <Typography variant="body2" sx={{ fontFamily: 'monospace', mb: 1, color: 'text.primary' }}>
        <strong>Service UUID:</strong> a8f3d5e0-8b2c-4f1a-9d6e-3c7b4a5f1e8d
      </Typography>
      <Typography variant="body2" sx={{ fontFamily: 'monospace', color: 'text.primary' }}>
        <strong>Characteristic UUID:</strong> a8f3d5e1-8b2c-4f1a-9d6e-3c7b4a5f1e8d
      </Typography>
    </Paper>

    <Typography variant="h6" gutterBottom>
      Supported Operations
    </Typography>
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText primary="READ" secondary="Read the current display state" />
      </ListItem>
      <ListItem>
        <ListItemText primary="WRITE" secondary="Update the display state" />
      </ListItem>
      <ListItem>
        <ListItemText primary="NOTIFY" secondary="Receive updates when display state changes" />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom>
      Data Format
    </Typography>
    <Typography variant="body2" paragraph>
      The characteristic uses JSON format:
    </Typography>
    <Paper elevation={1} sx={{ p: 2, bgcolor: 'background.paper', mb: 2 }}>
      <pre style={{ margin: 0, fontFamily: 'monospace', fontSize: '12px', color: 'inherit' }}>
{`{
  "line1": "Hello World",
  "line2": "From BLE",
  "i2c_address": 39,
  "backlight": true
}`}
      </pre>
    </Paper>

    <Typography variant="h6" gutterBottom>
      Connection Steps
    </Typography>
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText primary="1. Enable BLE" secondary="Enable BLE in the BLE Settings page" />
      </ListItem>
      <ListItem>
        <ListItemText primary="2. Scan for device" secondary="Use a BLE scanner app (e.g., nRF Connect) on your phone" />
      </ListItem>
      <ListItem>
        <ListItemText primary="3. Connect" secondary='Look for device named "Weighsoft-[device-id]"' />
      </ListItem>
      <ListItem>
        <ListItemText primary="4. Find service" secondary="Find the Display service using the Service UUID above" />
      </ListItem>
      <ListItem>
        <ListItemText primary="5. Use characteristic" secondary="Read/Write/Subscribe to the characteristic" />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom>
      Notes
    </Typography>
    <List>
      <ListItem>
        <ListItemText primary="I2C address is sent as decimal (39 = 0x27, 63 = 0x3F)" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Text longer than 16 characters will be truncated by the device" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Changes via BLE are synchronized to all other channels (REST, WebSocket, MQTT)" />
      </ListItem>
    </List>
  </SectionContent>
);

export default LcdDisplayBle;
