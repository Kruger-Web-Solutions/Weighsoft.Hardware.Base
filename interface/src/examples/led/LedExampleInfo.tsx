import { FC } from 'react';

import { Typography, Box, List, ListItem, ListItemText, Alert } from '@mui/material';

import { SectionContent } from '../../components';

const LedExampleInfo: FC = () => (
  <SectionContent title='LED Example Information' titleGutter>
    <Alert severity="info" sx={{ mb: 2 }}>
      This is an example project demonstrating Weighsoft's single-layer architecture pattern.
      Use it as a template for building your own industrial IoT services.
    </Alert>
    
    <Typography variant="body1" paragraph>
      This LED example demonstrates how to build a complete IoT service with four communication channels:
    </Typography>
    
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText
          primary="REST API"
          secondary="HTTP-based control for integration with web apps and external systems"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="WebSocket"
          secondary="Real-time bidirectional communication for instant updates"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="MQTT"
          secondary="Pub/sub messaging for IoT integrations like Home Assistant"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="BLE"
          secondary="Bluetooth Low Energy for mobile app integration and local device control"
        />
      </ListItem>
    </List>
    
    <Typography variant="h6" gutterBottom>
      Single-Layer Architecture
    </Typography>
    <Typography variant="body1" paragraph>
      This example follows Weighsoft's simplified single-layer pattern:
    </Typography>
    <List sx={{ mb: 2 }}>
      <ListItem>
        <ListItemText
          primary="Inline Configuration"
          secondary="Protocol settings (MQTT topics, BLE UUIDs) are defined directly in the service - no separate settings services"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Multi-Channel Sync"
          secondary="All channels (REST, WebSocket, MQTT, BLE) share the same state and automatically broadcast changes"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Origin Tracking"
          secondary="StatefulService prevents feedback loops - when you change the LED via WebSocket, it broadcasts to MQTT but not back to WebSocket"
        />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom>
      File Structure
    </Typography>
    <Typography variant="body2" component="div" paragraph>
      Backend: <code>src/examples/led/</code>
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText primary="LedExampleService.h/cpp" secondary="Service implementing LED control with inline MQTT configuration" />
      </ListItem>
    </List>
    
    <Typography variant="body2" component="div" paragraph sx={{ mt: 2 }}>
      Frontend: <code>interface/src/examples/led/</code>
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText primary="LedExample.tsx" secondary="Main router with tabs for each control method" />
      </ListItem>
      <ListItem>
        <ListItemText primary="LedControlRest.tsx" secondary="REST API control form" />
      </ListItem>
      <ListItem>
        <ListItemText primary="LedControlWebSocket.tsx" secondary="WebSocket control with live updates" />
      </ListItem>
      <ListItem>
        <ListItemText primary="LedControlBle.tsx" secondary="Bluetooth Low Energy control and device management" />
      </ListItem>
    </List>

    <Box mt={3}>
      <Typography variant="body2">
        For detailed implementation guidance, see <code>docs/LED-EXAMPLE.md</code> and <code>docs/DESIGN-PATTERNS.md</code>.
      </Typography>
    </Box>
  </SectionContent>
);

export default LedExampleInfo;
