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
    <Alert severity="info" sx={{ mb: 2 }}>
      <Typography variant="body2">
        <strong>Reader = scale in.</strong> A scale is wired to this ESP. The ESP listens on its RX pin,
        finds the weight in each line and shares it over Wi-Fi (this page, MQTT, live stream, and any
        Writer that connects to it).
      </Typography>
    </Alert>
    <Typography variant="body1" paragraph>
      This service listens on Serial1 and streams each line in real time. You can set the baud rate,
      data bits, stop bits, parity, and a pattern that picks the weight out of the line.
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
      Wiring (Reader)
    </Typography>
    <Typography variant="body2">
      The scale talks, the ESP listens. Cross the wires: scale TX goes to ESP RX.
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText
          primary="Scale TX → ESP RX"
          secondary="ESP32-S3 boards: GPIO18. Yellow 2.8 inch CYD board: GPIO22 on the CN1 plug."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="ESP TX (not needed)"
          secondary="ESP32-S3 boards: GPIO17. CYD: GPIO27 on CN1. The Reader only listens, so this wire can stay off."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="GND → GND"
          secondary="Always join the grounds."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="RS232 scale? Use a converter"
          secondary={'A real RS232 scale swings up to ±12 V. Put a 3.3 V RS232-to-TTL converter (MAX3232) '
            + 'between the scale and the ESP pins.'}
        />
      </ListItem>
    </List>
  </SectionContent>
  );
};

export default SerialInfo;
