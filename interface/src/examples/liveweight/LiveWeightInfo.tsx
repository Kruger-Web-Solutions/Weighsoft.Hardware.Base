import { FC } from 'react';

import { Alert, List, ListItem, ListItemText, Typography } from '@mui/material';

import { SectionContent } from '../../components';

const LiveWeightInfo: FC = () => (
  <SectionContent title="How weight gets in" titleGutter>
    <Typography paragraph>
      Live Weight is split into operator tabs. Pick the weight source on <strong>Tech</strong>. Everyday job settings
      live on <strong>Target & Relays</strong> and <strong>Product</strong>. The <strong>Live</strong> tab is the
      instrument only.
    </Typography>

    <List dense>
      <ListItem>
        <ListItemText
          primary="Live"
          secondary="Hero dial, Net weight, Gross/Zone chips, and a read-only PLU · product · count · total strip."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Target & Relays"
          secondary={
            'Target range, UNDER/CORRECT/OVER → RY mapping, DI1/DI2 actions (Print / Next / Start / Stop), ' +
            'and network printer IP:port.'
          }
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Product"
          secondary="PLU, description, count, and unit. Total is derived on the board."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Tech (admin)"
          secondary="Serial (RS-232) or WiFi source, baud, optional regex, test weight, and Connect/Stop live stream."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Serial (RS-232)"
          secondary="Scale indicator → MAX3232 / PROG header → UART0. Set baud and regex on Tech."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="WiFi / WebSocket"
          secondary="POST JSON { weight, last_line } to /rest/liveWeight, or push the same fields on /ws/liveWeight."
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="How senders find the board"
          secondary={
            'Board announces on UDP :4210 (service weighsoft-lw) and mDNS _weighsoft-lw._tcp. ' +
            'Any LAN sender can auto-adopt; manual IP is configured on the sender only. See Tech tab.'
          }
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="RS-485"
          secondary="Not available on this board — no transceiver / free pins. Do not select as a working source."
        />
      </ListItem>
    </List>

    <Alert severity="info" sx={{ mt: 2 }}>
      Endpoints: GET/POST /rest/liveWeight · WebSocket /ws/liveWeight
    </Alert>
  </SectionContent>
);

export default LiveWeightInfo;
