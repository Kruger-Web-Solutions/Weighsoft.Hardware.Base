import React, { FC } from 'react';
import { Typography, List, ListItem, ListItemText, Box, Paper } from '@mui/material';
import { SectionContent } from '../../components';
import LoopIcon from '@mui/icons-material/Loop';
import SearchIcon from '@mui/icons-material/Search';
import SignalCellularAltIcon from '@mui/icons-material/SignalCellularAlt';

const DiagnosticsInfo: FC = () => (
  <SectionContent title="UART Diagnostics Tools" titleGutter>
    <Typography variant="body1" paragraph>
      Use these tools to troubleshoot serial connectivity issues before debugging
      scale wiring or software. All tests use GPIO18 (RX) and GPIO17 (TX) on Serial1.
    </Typography>

    <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
      Available Tests
    </Typography>

    <Paper sx={{ p: 2, mb: 2 }}>
      <Box sx={{ display: 'flex', alignItems: 'center', mb: 1 }}>
        <LoopIcon sx={{ mr: 1, color: 'primary.main' }} />
        <Typography variant="subtitle1" fontWeight="bold">
          Loopback Test
        </Typography>
      </Box>
      <Typography variant="body2" paragraph>
        Verifies that GPIO18 (RX) and GPIO17 (TX) hardware is functional by sending data
        from TX to RX. This confirms the ESP32 serial hardware is working correctly.
      </Typography>
      <Typography variant="body2" color="text.secondary">
        <strong>Setup:</strong> Connect GPIO18 (RX) to GPIO17 (TX) with a jumper wire
      </Typography>
    </Paper>

    <Paper sx={{ p: 2, mb: 2 }}>
      <Box sx={{ display: 'flex', alignItems: 'center', mb: 1 }}>
        <SearchIcon sx={{ mr: 1, color: 'primary.main' }} />
        <Typography variant="subtitle1" fontWeight="bold">
          Baud Rate Detection
        </Typography>
      </Box>
      <Typography variant="body2" paragraph>
        Automatically finds your scale&apos;s baud rate by testing common values
        (1200-115200). Useful when the device baud rate is unknown.
      </Typography>
      <Typography variant="body2" color="text.secondary">
        <strong>Setup:</strong> Connect scale TX to ESP32 GPIO18 (RX) (must be actively transmitting data)
      </Typography>
    </Paper>

    <Paper sx={{ p: 2, mb: 2 }}>
      <Box sx={{ display: 'flex', alignItems: 'center', mb: 1 }}>
        <SignalCellularAltIcon sx={{ mr: 1, color: 'primary.main' }} />
        <Typography variant="subtitle1" fontWeight="bold">
          Signal Quality Analysis
        </Typography>
      </Box>
      <Typography variant="body2" paragraph>
        Measures serial connection reliability and performance by sending test
        packets and calculating success rate, latency, and jitter.
      </Typography>
      <Typography variant="body2" color="text.secondary">
        <strong>Setup:</strong> Connect GPIO18 (RX) to GPIO17 (TX) with a jumper wire (loopback)
      </Typography>
    </Paper>

    <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
      Common Use Cases
    </Typography>
    <List>
      <ListItem>
        <ListItemText
          primary="Scale not sending data"
          secondary="Run loopback test first to verify ESP32 hardware is working"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Unknown baud rate"
          secondary="Run baud rate detector with scale connected and transmitting"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="Intermittent errors or data corruption"
          secondary="Run signal quality test to measure connection reliability"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="New hardware setup"
          secondary="Run all tests in sequence: loopback → baud detection → signal quality"
        />
      </ListItem>
    </List>

    <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
      Hardware Pins
    </Typography>
    <List dense>
      <ListItem>
        <ListItemText
          primary="GPIO18 (RX)"
          secondary="Receives data from external device TX"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="GPIO17 (TX)"
          secondary="Transmits data to external device RX (or loopback to GPIO18)"
        />
      </ListItem>
      <ListItem>
        <ListItemText
          primary="GND"
          secondary="Common ground with external device (important!)"
        />
      </ListItem>
    </List>
  </SectionContent>
);

export default DiagnosticsInfo;
