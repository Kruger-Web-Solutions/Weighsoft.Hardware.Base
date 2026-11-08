import { FC } from 'react';

import { Alert, List, ListItem, ListItemText, Typography } from '@mui/material';

import { SectionContent } from '../../components';

const WSoftInfo: FC = () => (
  <SectionContent title="How W-Soft connects" titleGutter>
    <Typography paragraph>
      This board is the W-Soft weigh screen: a Waveshare ESP32-S3 4.3 inch touch LCD reading a DJB-M1X junction box
      over RS485. The screen works on its own; these pages only show what it shows.
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText
          primary="Live"
          secondary={
            'Weight, stable flag and audit counter, plus the Approval and Auto save switches. ' +
            'An admin switches them here; Approval asks for a reason and logs the admin name.'
          }
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Scale setup"
          secondary={
            'RS485: SA number, Max, Min, blanking, stable samples and tare on the board, plus the weighing card ' +
            'settings. Locked while Approval is on. RS232: set on the indicator.'
          }
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Settings (admin)"
          secondary="Publish interval, the MQTT topic for W-Soft status, and the Approval PIN for the screen."
        />
      </ListItem>
      <ListItem>
        <ListItemText primary="Hub (admin)" secondary="Connect to the Hub with a pairing key, see the link state, or disconnect." />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="USB cable (technician)"
          secondary="APPROVAL STATUS / LOG / ON / OFF and HUB PAIR / SAVE / CLEAR."
        />
      </ListItem>
    </List>
    <Alert severity="info" sx={{ mt: 2 }}>
      Endpoints: GET /rest/wsoftStatus · WebSocket /ws/wsoftStatus · MQTT &lt;base path&gt;/status ·
      GET/POST /rest/wsoftSettings (admin) · GET/POST /rest/wsoftHub (admin) · GET /rest/wsoftSwitches ·
      POST /rest/wsoftSwitches (admin) · POST /rest/wsoftPin (admin) · GET /rest/wsoftScaleSetup ·
      POST /rest/wsoftScaleSetup (admin) · POST /rest/djb (admin)
    </Alert>
  </SectionContent>
);

export default WSoftInfo;
