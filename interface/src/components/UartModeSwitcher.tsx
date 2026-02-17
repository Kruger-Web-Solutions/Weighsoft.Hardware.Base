import React from 'react';
import { Alert, ToggleButtonGroup, ToggleButton, Box, Typography, CircularProgress } from '@mui/material';
import MonitorIcon from '@mui/icons-material/Monitor';
import BuildIcon from '@mui/icons-material/Build';
import { useRest } from '../utils';
import { readUartMode, updateUartMode } from '../api/uartMode';
import { UartModeData } from '../types/uartMode';

interface UartModeSwitcherProps {
  currentMode: 'live' | 'diagnostics';
}

const UartModeSwitcher: React.FC<UartModeSwitcherProps> = ({ currentMode }) => {
  const {
    loadData,
    saving,
    data,
    errorMessage
  } = useRest<UartModeData>({ read: readUartMode, update: updateUartMode });

  React.useEffect(() => {
    loadData();
  }, [loadData]);

  const handleModeChange = async (event: React.MouseEvent<HTMLElement>, newMode: 'live' | 'diagnostics' | null) => {
    if (newMode && newMode !== data?.mode) {
      try {
        await updateUartMode({ mode: newMode });
        await loadData();
      } catch (error) {
        console.error('Failed to switch mode:', error);
      }
    }
  };

  const activeMode = data?.mode || currentMode;

  return (
    <Box mb={3}>
      <Alert severity="info" sx={{ mb: 2 }}>
        <Typography variant="body2">
          <strong>UART Mode:</strong> {activeMode === 'live' ? 'Live Monitoring' : 'Diagnostics Testing'}
        </Typography>
        <Typography variant="caption" color="text.secondary">
          Only one mode can use the Serial2 (GPIO17/18) hardware at a time.
        </Typography>
      </Alert>

      <Box display="flex" alignItems="center" gap={2}>
        <ToggleButtonGroup
          value={activeMode}
          exclusive
          onChange={handleModeChange}
          aria-label="UART mode"
          disabled={saving}
          fullWidth
        >
          <ToggleButton value="live" aria-label="live monitoring">
            <MonitorIcon sx={{ mr: 1 }} />
            <Box>
              <Typography variant="button">Live Monitoring</Typography>
              <Typography variant="caption" display="block" sx={{ textTransform: 'none' }}>
                Scale data streaming
              </Typography>
            </Box>
          </ToggleButton>
          <ToggleButton value="diagnostics" aria-label="diagnostics">
            <BuildIcon sx={{ mr: 1 }} />
            <Box>
              <Typography variant="button">Diagnostics</Typography>
              <Typography variant="caption" display="block" sx={{ textTransform: 'none' }}>
                Hardware testing
              </Typography>
            </Box>
          </ToggleButton>
        </ToggleButtonGroup>
        {saving && <CircularProgress size={24} />}
      </Box>

      {errorMessage && (
        <Alert severity="error" sx={{ mt: 2 }}>
          {errorMessage}
        </Alert>
      )}

      {activeMode !== currentMode && (
        <Alert severity="warning" sx={{ mt: 2 }}>
          Mode has changed. Current screen is for <strong>{currentMode}</strong> mode,
          but system is in <strong>{activeMode}</strong> mode.
        </Alert>
      )}
    </Box>
  );
};

export default UartModeSwitcher;
