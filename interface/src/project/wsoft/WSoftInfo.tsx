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
        <ListItemText primary="Live" secondary="Weight in whole grams, stable flag, approval state and audit counter." />
      </ListItem>
      <ListItem>
        <ListItemText primary="Settings (admin)" secondary="Publish interval and the MQTT topic for W-Soft status." />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="USB cable (technician)"
          secondary="APPROVAL STATUS / LOG / ON / OFF and HUB PAIR / SAVE / CLEAR. Approval is never switched from here."
        />
      </ListItem>
    </List>
    <Alert severity="info" sx={{ mt: 2 }}>
      Endpoints: GET /rest/wsoftStatus · WebSocket /ws/wsoftStatus · MQTT &lt;base path&gt;/status ·
      GET/POST /rest/wsoftSettings (admin)
    </Alert>
  </SectionContent>
);

export default WSoftInfo;
