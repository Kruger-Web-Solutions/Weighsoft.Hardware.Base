import React, { useState } from 'react';
import {
  Alert, ToggleButtonGroup, ToggleButton, Box, Typography, CircularProgress,
  Dialog, DialogTitle, DialogContent, DialogActions, Button
} from '@mui/material';
import MonitorIcon from '@mui/icons-material/Monitor';
import SendIcon from '@mui/icons-material/Send';
import BuildIcon from '@mui/icons-material/Build';
import { useRest } from '../utils';
import { readUartMode, updateUartMode } from '../api/uartMode';
import { UartMode, UartModeData } from '../types/uartMode';

interface UartModeSwitcherProps {
  currentMode?: UartMode;  // shown for cross-mode mismatch banner; optional
}

const labels: Record<Exclude<UartMode, ''>, string> = {
  reader: 'Reader',
  writer: 'Writer',
  diagnostics: 'Diagnostics'
};

const UartModeSwitcher: React.FC<UartModeSwitcherProps> = ({ currentMode }) => {
  const { loadData, saving, data, errorMessage } = useRest<UartModeData>({ read: readUartMode, update: updateUartMode });
  const [pending, setPending] = useState<Exclude<UartMode, ''> | null>(null);

  React.useEffect(() => { loadData(); }, [loadData]);

  const active = (data?.mode || 'reader') as Exclude<UartMode, ''>;

  const onChange = (_: React.MouseEvent<HTMLElement>, newMode: Exclude<UartMode, ''> | null) => {
    if (newMode && newMode !== active) setPending(newMode);
  };

  const confirm = async () => {
    if (!pending) return;
    try {
      await updateUartMode({ mode: pending });
      await loadData();
    } finally {
      setPending(null);
    }
  };

  return (
    <Box mb={3}>
      {!data?.mode && (
        <Alert severity="info" sx={{ mb: 2 }}>
          <Typography variant="body2"><strong>First-time setup:</strong> pick a mode below to get started.</Typography>
        </Alert>
      )}

      <Box display="flex" alignItems="center" gap={2}>
        <ToggleButtonGroup value={active} exclusive onChange={onChange} aria-label="UART mode" disabled={saving} fullWidth>
          <ToggleButton value="reader" aria-label="reader"><MonitorIcon sx={{ mr: 1 }} />Reader</ToggleButton>
          <ToggleButton value="writer" aria-label="writer"><SendIcon sx={{ mr: 1 }} />Writer</ToggleButton>
          <ToggleButton value="diagnostics" aria-label="diagnostics"><BuildIcon sx={{ mr: 1 }} />Diagnostics</ToggleButton>
        </ToggleButtonGroup>
        {saving && <CircularProgress size={24} />}
      </Box>

      {errorMessage && <Alert severity="error" sx={{ mt: 2 }}>{errorMessage}</Alert>}

      {currentMode && data?.mode && currentMode !== data.mode && (
        <Alert severity="warning" sx={{ mt: 2 }}>
          Mode has changed. Current screen is for <strong>{currentMode}</strong>, but device is in <strong>{data.mode}</strong>.
        </Alert>
      )}

      <Dialog open={pending !== null} onClose={() => setPending(null)}>
        <DialogTitle>Switch to {pending && labels[pending]}?</DialogTitle>
        <DialogContent>
          <Typography>The current mode will pause and the new mode will take over the serial port. Saved settings are kept.</Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setPending(null)}>Cancel</Button>
          <Button onClick={confirm} variant="contained">Switch</Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
};

export default UartModeSwitcher;
