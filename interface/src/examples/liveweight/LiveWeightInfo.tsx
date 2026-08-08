import { FC } from 'react';

import { Alert, List, ListItem, ListItemText, Typography } from '@mui/material';

import { SectionContent } from '../../components';

const LiveWeightInfo: FC = () => (
  <SectionContent title="How weight gets in" titleGutter>
    <Typography paragraph>
      One Live Weight screen. The board can take the same weight number from different paths — pick the source on the
      Live tab.
    </Typography>

    <List dense>
      <ListItem>
        <ListItemText
          primary="Serial (RS-232)"
          secondary="Scale indicator → MAX3232 / PROG header → UART0. Set baud and regex on the Live tab. Reuses the serial reader pattern from the serial2 branch."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="WiFi / WebSocket"
          secondary="POST JSON { weight, last_line } to /rest/liveWeight, or push the same fields on /ws/liveWeight. MQTT subscribe topic weighsoft/liveWeight/<id>/set. Same idea as RemoteWeight on the serial branch."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="RS-485"
          secondary="Stubbed. Needs an RS-485 transceiver on the board. Address is saved so we can finish the reader without changing the UI again."
        />
      </ListItem>
    </List>

    <Alert severity="info" sx={{ mt: 2 }}>
      Endpoints: GET/POST /rest/liveWeight · WebSocket /ws/liveWeight · MQTT pub …/data, sub …/set
    </Alert>
  </SectionContent>
);

export default LiveWeightInfo;
