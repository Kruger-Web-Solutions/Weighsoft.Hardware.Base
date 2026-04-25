import { FC } from 'react';
import { Typography, Box, List, ListItem, ListItemText, Alert } from '@mui/material';

import { SectionContent } from '../../components';

const LedControlBle: FC = () => (
  <SectionContent title='BLE Control' titleGutter>
    <Alert severity="info" sx={{ mb: 2 }}>
      Control the LED via Bluetooth Low Energy using nRF Connect or any BLE-capable app.
    </Alert>
    
    <Typography variant="h6" gutterBottom>
      Connection Details
    </Typography>
    
    <List>
      <ListItem>
        <ListItemText
          primary="Service UUID"
          secondary="19b10000-e8f2-537e-4f6c-d104768a1214"
          secondaryTypographyProps={{ fontFamily: 'monospace' }}
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Characteristic UUID"
          secondary="19b10001-e8f2-537e-4f6c-d104768a1214"
          secondaryTypographyProps={{ fontFamily: 'monospace' }}
        />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom sx={{ mt: 2 }}>
      JSON Format
    </Typography>
    
    <Box sx={{ bgcolor: 'background.default', p: 2, borderRadius: 1, mb: 2 }}>
      <Typography component="pre" variant="body2" sx={{ fontFamily: 'monospace' }}>
        {`{
  "led_on": true
}`}
      </Typography>
    </Box>

    <Typography variant="h6" gutterBottom>
      Using nRF Connect (Mobile)
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText primary="1. Install nRF Connect app (iOS/Android)" />
      </ListItem>
      <ListItem>
        <ListItemText primary="2. Scan for 'Weighsoft-' devices" />
      </ListItem>
      <ListItem>
        <ListItemText primary="3. Connect to the device" />
      </ListItem>
      <ListItem>
        <ListItemText primary="4. Find the LED Control service (UUID above)" />
      </ListItem>
      <ListItem>
        <ListItemText primary="5. Write JSON to characteristic to control LED" />
      </ListItem>
      <ListItem>
        <ListItemText primary="6. Enable notifications to receive state updates" />
      </ListItem>
    </List>

    <Alert severity="success" sx={{ mt: 2 }}>
      <Typography variant="body2">
        Changes from any channel (REST, WebSocket, MQTT, BLE) automatically sync to all other channels!
      </Typography>
    </Alert>
  </SectionContent>
);

export default LedControlBle;
