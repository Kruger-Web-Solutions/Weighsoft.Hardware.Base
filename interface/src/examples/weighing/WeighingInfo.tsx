import React, { FC } from 'react';
import {
  Typography,
  List,
  ListItem,
  ListItemText,
  Alert,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  Link,
} from '@mui/material';
import { Link as RouterLink } from 'react-router-dom';
import { SectionContent } from '../../components';

const WeighingInfo: FC = () => (
  <SectionContent title="Weighing Board" titleGutter>
    <Alert severity="info" sx={{ mb: 2 }}>
      Reads weight from an ADS1231 24-bit load cell ADC and streams live data via REST,
      WebSocket, MQTT, and BLE. Designed for OIML D31:2023 type approval.
    </Alert>

    <Typography variant="h6" gutterBottom>
      Features
    </Typography>
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText primary="Live weight reading" secondary="Gross, net, and tare display with stability detection" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Semi-automatic and preset tare" secondary="Place container, press Tare; or enter known container weight" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Manual zero and automatic zero tracking (AZT)" secondary="Correct drift to zero within configured range" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Span calibration" secondary="Place known weight, enter value, calculate calibration factor" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Electronic seal (OIML 6.2.3.4)" secondary="Factory PIN protects calibration from accidental changes" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Audit trail (OIML 6.2.3.6)" secondary="Append-only log of all legally relevant events with NTP timestamps" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Defect detection (OIML 6.2.6.1)" secondary="ADC connection loss, overload, and underload reported on all channels" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Multi-channel" secondary="REST API, WebSocket stream, MQTT, and BLE" />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom>
      Hardware Setup
    </Typography>
    <Typography variant="body2" paragraph>
      Connect the ADS1231 24-bit load cell ADC to your ESP32:
    </Typography>

    <Table sx={{ mb: 2 }}>
      <TableHead>
        <TableRow>
          <TableCell><strong>ADS1231 Pin</strong></TableCell>
          <TableCell><strong>ESP32 GPIO</strong></TableCell>
          <TableCell><strong>Function</strong></TableCell>
        </TableRow>
      </TableHead>
      <TableBody>
        <TableRow>
          <TableCell>DOUT</TableCell>
          <TableCell>GPIO25</TableCell>
          <TableCell>Data out (serial data, MISO)</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>SCLK</TableCell>
          <TableCell>GPIO23</TableCell>
          <TableCell>Serial clock (bit-banged)</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>PDWN</TableCell>
          <TableCell>GPIO22</TableCell>
          <TableCell>Power down (HIGH = normal)</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>SPEED</TableCell>
          <TableCell>GPIO19</TableCell>
          <TableCell>Output rate (LOW=10SPS, HIGH=80SPS)</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>VCC</TableCell>
          <TableCell>3.3V or 5V</TableCell>
          <TableCell>Power supply</TableCell>
        </TableRow>
        <TableRow>
          <TableCell>GND</TableCell>
          <TableCell>GND</TableCell>
          <TableCell>Common ground</TableCell>
        </TableRow>
      </TableBody>
    </Table>

    <Typography variant="h6" gutterBottom>
      Getting Started
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText
          primary="1. Use the Configuration tab to set max capacity, min division, and unit"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="2. Use the Calibration tab to perform a two-point calibration (zero + known weight)"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="3. Use the Live Stream tab for real-time weight, tare, and zero operations"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="4. Seal the instrument in the Calibration tab once calibration is complete"
        />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
      Type Approval (OIML D31:2023)
    </Typography>
    <Typography variant="body2" paragraph>
      This service is designed to meet OIML D31:2023 requirements for software-controlled
      weighing instruments. Key compliance features:
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText primary="Software identification: version accessible at GET /rest/version" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Event counter: non-resettable, incremented on every legally relevant change" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Audit trail: /audit/weighing.json, survives OTA firmware updates" />
      </ListItem>
      <ListItem>
        <ListItemText primary="Electronic seal: factory-programmed PIN protects calibration parameters" />
      </ListItem>
    </List>

    <Typography variant="body2">
      See the{' '}
      <Link component={RouterLink} to="../calibration">Calibration</Link> and{' '}
      <Link component={RouterLink} to="../audit">Audit Trail</Link> tabs for legally relevant controls.
    </Typography>
  </SectionContent>
);

export default WeighingInfo;
