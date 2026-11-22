import React, { FC } from 'react';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  Alert,
  Typography,
  List,
  ListItem,
  ListItemText,
} from '@mui/material';

import { SectionContent } from '../../components';

const LcdDisplayInfo: FC = () => (
  <SectionContent title="Display" titleGutter>
    <Alert severity="info" sx={{ mb: 2 }}>
      Control a 2-row LCD display (YwRobot LCM1602) via REST, WebSocket, MQTT, and BLE.
    </Alert>

    <Typography variant="h6" gutterBottom>
      Features
    </Typography>
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText primary="16 characters x 2 rows display" />
      </ListItem>
      <ListItem>
        <ListItemText primary="I2C interface (configurable address)" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Backlight control" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Text truncation for lines longer than 16 characters" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Multi-channel control (REST, WebSocket, MQTT, BLE)" />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom>
      Hardware Setup
    </Typography>
    <Typography variant="body1" paragraph>
      Connect the YwRobot LCM1602 LCD to your ESP32/ESP8266 using I2C:
    </Typography>

    <Table sx={{ mb: 2 }}>
      <TableHead>
        <TableRow>
          <TableCell>LCD Pin</TableCell>
          <TableCell>ESP32 Pin</TableCell>
          <TableCell>ESP8266 Pin</TableCell>
        </TableRow>
      </TableHead>
      <TableBody>
        <TableRow>
          <TableCell>SDA</TableCell>
          <TableCell>GPIO21</TableCell>
          <TableCell>GPIO4 (D2)</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>SCL</TableCell>
          <TableCell>GPIO22</TableCell>
          <TableCell>GPIO5 (D1)</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>VCC</TableCell>
          <TableCell>5V</TableCell>
          <TableCell>5V</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>GND</TableCell>
          <TableCell>GND</TableCell>
          <TableCell>GND</TableCell>
        </TableRow>
      </TableBody>
    </Table>

    <Typography variant="h6" gutterBottom>
      I2C Address Configuration
    </Typography>
    <Typography variant="body1" paragraph>
      The most common I2C addresses for the YwRobot LCM1602 are:
    </Typography>
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText primary="0x27 (default)" />
      </ListItem>
      <ListItem>
        <ListItemText primary="0x3F (alternative)" />
      </ListItem>
    </List>
    <Typography variant="body2" paragraph>
      You can change the I2C address via the Control tab if your display uses a different address.
    </Typography>

    <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
      Serial Bridge
    </Typography>
    <Typography variant="body2" paragraph>
      The Display can connect to a Serial device (from the 'serial' branch) to show
      streaming serial data on the LCD. Three connection modes are available:
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText
          primary="WebSocket Mode"
          secondary="Direct connection to Serial device via IP address. Low latency, best for single display."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="MQTT Mode"
          secondary="Subscribe to Serial device's MQTT topic. No IP needed, works with multiple displays."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="BLE Mode"
          secondary="Bluetooth connection to Serial device. Works without WiFi, automatic device discovery."
        />
      </ListItem>
    </List>
    <Typography variant="body2" paragraph>
      Configure in the "Serial Bridge" tab.
    </Typography>

    <Typography variant="h6" gutterBottom>
      Use Cases
    </Typography>
    <List>
      <ListItem>
        <ListItemText primary="Display system status messages" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Show sensor readings" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Display notifications" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Show time/date information" />
      </ListItem>
    </List>
  </SectionContent>
);

export default LcdDisplayInfo;
