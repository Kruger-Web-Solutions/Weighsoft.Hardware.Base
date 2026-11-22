import React, { FC } from 'react';
import { Typography, List, ListItem, ListItemText } from '@mui/material';
import { SectionContent } from '../../components';

const SerialBle: FC = () => (
  <SectionContent title='BLE Stream' titleGutter>
    <Typography variant="body2" paragraph>
      Monitor serial data via Bluetooth Low Energy using a BLE app like nRF Connect.
    </Typography>

    <Typography variant="h6" gutterBottom>
      Connection Details
    </Typography>
    <List>
      <ListItem>
        <ListItemText
          primary="Service UUID"
          secondary="12340000-e8f2-537e-4f6c-d104768a1234"
          secondaryTypographyProps={{ fontFamily: 'monospace' }}
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Characteristic UUID"
          secondary="12340001-e8f2-537e-4f6c-d104768a1234"
          secondaryTypographyProps={{ fontFamily: 'monospace' }}
        />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
      Using nRF Connect
    </Typography>
    <List dense>
      <ListItem>1. Scan for devices, connect to your ESP32</ListItem>
      <ListItem>2. Find the Serial service (UUID above)</ListItem>
      <ListItem>3. Enable notifications on the characteristic</ListItem>
      <ListItem>
        4. Data will stream as JSON: {`{"last_line":"...","weight":"...","timestamp":...}`}
      </ListItem>
    </List>
  </SectionContent>
);

export default SerialBle;
