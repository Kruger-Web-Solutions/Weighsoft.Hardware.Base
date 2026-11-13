import React, { useContext, useState } from 'react';
import {
  Alert, ToggleButtonGroup, ToggleButton, Box, Typography, CircularProgress,
  Dialog, DialogTitle, DialogContent, DialogActions, Button
} from '@mui/material';
import MonitorIcon from '@mui/icons-material/Monitor';
import SendIcon from '@mui/icons-material/Send';
import { useRest } from '../utils';
import { readUartMode, updateUartMode } from '../api/uartMode';
import { UartMode, UartModeData } from '../types/uartMode';
import { FeaturesContext } from '../contexts/features';

interface UartModeSwitcherProps {
  currentMode?: UartMode;  // shown for cross-mode mismatch banner; optional
}

const labels: Record<Exclude<UartMode, ''>, string> = {
  reader: 'Reader (scale in)',
  writer: 'Writer (data out)'
};

// One line per role, in plain words. Pins: SerialService.h (SERIAL2_RX_PIN, 18 on
// ESP32-S3 boards, 22 on the CYD CN1 plug) and SerialWriterService.h (SERIALW_TX_PIN 17).
const roleHints: Record<Exclude<UartMode, ''>, string> = {
  reader: 'Reader = scale in. A scale is wired to this ESP. It listens on its RX pin, finds the weight '
    + 'and shares it over Wi-Fi.',
  writer: 'Writer = data out. This ESP gets the lines from a Reader over Wi-Fi and sends them out of its TX pin '
    + 'to a printer, big display or PC.'
};

// W-Soft (Rhyno, 12 Nov 2026): one device with a switch. Weigh reads the scale and saves;
// Writer follows a weigh board over Wi-Fi and sends its weight out of the port. The Hub link
// stays with the board either way, and a Writer saves nothing, so nothing is counted twice.
const wsoftLabels: Record<Exclude<UartMode, ''>, string> = {
  reader: 'Weigh (reads the scale)',
  writer: 'Writer (follows a weigh board)'
};

const wsoftHints: Record<Exclude<UartMode, ''>, string> = {
  reader: 'Weigh = this board reads its own scale, saves the weighings and sends them to the Hub. '
    + 'It also shares the weight over Wi-Fi, so a Writer can follow it.',
  writer: 'Writer = this board has no scale. It gets the weight from a weigh board over Wi-Fi and sends it out '
    + 'of its port to a printer, big display or PC. It saves no weighings. Its Hub link stays.'
};

const UartModeSwitcher: React.FC<UartModeSwitcherProps> = ({ currentMode }) => {
  const { features } = useContext(FeaturesContext);
  const wsoft = !!features.wsoft;
  const names = wsoft ? wsoftLabels : labels;
  const hints = wsoft ? wsoftHints : roleHints;
  const { loadData, saving, data, errorMessage } = useRest<UartModeData>({ read: readUartMode, update: updateUartMode });
  const [pending, setPending] = useState<Exclude<UartMode, ''> | null>(null);

  React.useEffect(() => { loadData(); }, [loadData]);

  // On W-Soft no saved role means Weigh.
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
      {!data?.mode && !wsoft && (
        <Alert severity="info" sx={{ mb: 2 }}>
          <Typography variant="body2">
            <strong>First-time setup:</strong> pick what this ESP does. Reader if a scale is wired to it,
            Writer if it must send the weight on to another device.
          </Typography>
        </Alert>
      )}

      <Box display="flex" alignItems="center" gap={2}>
        <ToggleButtonGroup value={active} exclusive onChange={onChange} aria-label="UART mode" disabled={saving} fullWidth>
          <ToggleButton value="reader" aria-label="reader"><MonitorIcon sx={{ mr: 1 }} />{names.reader}</ToggleButton>
          <ToggleButton value="writer" aria-label="writer"><SendIcon sx={{ mr: 1 }} />{names.writer}</ToggleButton>
        </ToggleButtonGroup>
        {saving && <CircularProgress size={24} />}
      </Box>

      <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
        {hints[active]}
      </Typography>

      {errorMessage && <Alert severity="error" sx={{ mt: 2 }}>{errorMessage}</Alert>}

      {!wsoft && currentMode && data?.mode && currentMode !== data.mode && (
        <Alert severity="warning" sx={{ mt: 2 }}>
          Mode has changed. Current screen is for <strong>{currentMode}</strong>, but device is in <strong>{data.mode}</strong>.
        </Alert>
      )}

      <Dialog open={pending !== null} onClose={() => setPending(null)}>
        <DialogTitle>Switch to {pending && names[pending]}?</DialogTitle>
        <DialogContent>
          <Typography paragraph>{pending && hints[pending]}</Typography>
          <Typography>
            {wsoft
              ? 'The board has one serial port, so it weighs or writes, never both. It restarts to switch. '
                + 'Saved settings and the Hub link are kept.'
              : 'The ESP has one serial port, so it is a Reader or a Writer, never both. The other role pauses. '
                + 'Saved settings are kept.'}
          </Typography>
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
