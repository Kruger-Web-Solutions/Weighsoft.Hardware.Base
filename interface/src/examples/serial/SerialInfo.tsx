import React, { FC } from 'react';
import { Typography, List, ListItem, ListItemText, Link, Alert } from '@mui/material';
import { Link as RouterLink } from 'react-router-dom';
import { SectionContent } from '../../components';
import { readKnownWriters } from '../../api/knownWriters';

const SerialInfo: FC = () => {
  const [connectedCount, setConnectedCount] = React.useState<number | null>(null);
  React.useEffect(() => {
    const tick = () => readKnownWriters()
      .then((r) => setConnectedCount(r.data.writers.filter((w) => w.online).length))
      .catch(() => setConnectedCount(null));
    tick();
    const id = setInterval(tick, 5000);
    return () => clearInterval(id);
  }, []);

  return (
  <SectionContent title="Serial" titleGutter>
      {connectedCount !== null && connectedCount > 0 && (
        <Alert severity="success" sx={{ mb: 2 }}>
          <Typography variant="body2">
            <strong>{connectedCount}</strong> Writer{connectedCount === 1 ? '' : 's'} currently connected.
          </Typography>
        </Alert>
      )}
    <Typography variant="body1" paragraph>
      This service monitors Serial1 (GPIO18 RX / GPIO17 TX on ESP32-S3) and streams the data
      across all communication channels in real-time. You can configure baud rate,
      data bits, stop bits, parity, and a regex pattern to extract weight values.
    </Typography>

    <Typography variant="h6" gutterBottom>
      Features
    </Typography>
    <List>
      <ListItem>
        <ListItemText
          primary="REST API"
          secondary="Returns the last received line and extracted weight (polling)"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="WebSocket"
          secondary="Real-time streaming of all lines as they arrive"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="MQTT"
          secondary="Publishes each line to topic: weighsoft/serial/{id}/data"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="BLE"
          secondary="Notifies subscribers when new lines arrive"
        />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
      Configuration
    </Typography>
    <Typography variant="body2" paragraph>
      Use the <Link component={RouterLink} to="../configuration">Configuration</Link> tab
      to set baud rate (e.g. 9600, 115200), data bits (7 or 8), stop bits, parity,
      and an optional regex pattern. The first capture group is used as the
      extracted weight and is sent together with the full line on all channels.
    </Typography>

    <Typography variant="h6" gutterBottom>
      Weight extraction (regex)
    </Typography>
    <Typography variant="body2" paragraph>
      If you set a regex pattern with one capture group (e.g. <code>(\d+\.\d+)</code>),
      the service will extract that value from each line and expose it as the
      &quot;weight&quot; field. If the pattern does not match, the full line is
      still sent and weight is empty. Example patterns: <code>(\d+\.?\d*)</code> for
      a decimal number, <code>Weight:\s*(\d+\.?\d*)</code> for &quot;Weight: 12.5&quot;.
    </Typography>

    <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
      Hardware Setup
    </Typography>
    <Typography variant="body2">
      Connect your serial device to:
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText
          primary="RX (GPIO18)"
          secondary="Receives data from external device TX"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="TX (GPIO17)"
          secondary="Not currently used by this service"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="GND"
          secondary="Common ground with external device"
        />
      </ListItem>
    </List>
  </SectionContent>
  );
};

export default SerialInfo;
