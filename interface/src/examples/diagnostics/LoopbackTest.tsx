import React, { FC } from 'react';
import {
  Typography,
  Box,
  Button,
  Alert,
  Grid,
  Card,
  CardContent,
  Chip,
  CircularProgress,
} from '@mui/material';
import { SectionContent } from '../../components';
import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { useWs } from '../../utils';
import { DiagnosticsData } from '../../types/diagnostics';
import { updateDiagnostics } from '../../api/diagnostics';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import StopIcon from '@mui/icons-material/Stop';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import ErrorIcon from '@mui/icons-material/Error';

export const DIAGNOSTICS_WEBSOCKET_URL = `${WEB_SOCKET_ROOT}diagnostics`;

const LoopbackTest: FC = () => {
  const { data, connected } = useWs<DiagnosticsData>(DIAGNOSTICS_WEBSOCKET_URL);

  const loopback = data?.loopback;
  const isRunning = loopback?.enabled ?? false;
  const status = loopback?.status ?? 'idle';
  const successRate = loopback?.success_rate ?? 0;

  const handleToggle = () => {
    updateDiagnostics({ loopback_enabled: !isRunning });
  };

  // Status indicator color
  const getStatusColor = () => {
    if (!isRunning) return 'text.secondary';
    if (status === 'pass') return 'success.main';
    if (status === 'fail') return 'error.main';
    return 'primary.main';
  };

  // Format uptime
  const formatUptime = (seconds: number) => {
    if (seconds < 60) return `${seconds}s`;
    const minutes = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${minutes}m ${secs}s`;
  };

  return (
    <SectionContent title="Loopback Test" titleGutter>
      {!connected && (
        <Alert severity="warning" sx={{ mb: 2 }}>
          Connecting to diagnostics service...
        </Alert>
      )}

      {/* Hardware Setup Instructions */}
      <Alert severity="info" sx={{ mb: 3 }}>
        <Typography variant="subtitle2" gutterBottom>
          Hardware Setup Required
        </Typography>
        <Typography variant="body2" paragraph sx={{ mb: 1 }}>
          <strong>1.</strong> Disconnect scale from GPIO18
          <br />
          <strong>2.</strong> Connect jumper wire: GPIO18 (RX) ↔ GPIO17 (TX)
          <br />
          <strong>3.</strong> Click &quot;Start Test&quot; below
        </Typography>
        <Typography variant="caption" color="text.secondary">
          Wiring: Connect GPIO18 directly to GPIO17 on the ESP32 board
        </Typography>
      </Alert>

      {/* Test Controls */}
      <Box sx={{ textAlign: 'center', my: 3 }}>
        <Button
          variant="contained"
          size="large"
          color={isRunning ? 'error' : 'primary'}
          startIcon={isRunning ? <StopIcon /> : <PlayArrowIcon />}
          onClick={handleToggle}
          disabled={!connected}
          sx={{ px: 4, py: 1.5 }}
        >
          {isRunning ? 'Stop Test' : 'Start Loopback Test'}
        </Button>
      </Box>

      {/* Status Display */}
      <Box sx={{ textAlign: 'center', mb: 3 }}>
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'center', mb: 1 }}>
          <Typography variant="body1" color="text.secondary" sx={{ mr: 1 }}>
            Status:
          </Typography>
          {isRunning && status === 'running' && <CircularProgress size={16} sx={{ mr: 1 }} />}
          {status === 'pass' && <CheckCircleIcon sx={{ mr: 1, color: 'success.main' }} />}
          {status === 'fail' && <ErrorIcon sx={{ mr: 1, color: 'error.main' }} />}
          <Typography
            variant="h6"
            fontWeight="bold"
            sx={{ color: getStatusColor(), textTransform: 'uppercase' }}
          >
            {isRunning ? status : 'Not Running'}
          </Typography>
        </Box>
        {isRunning && loopback && (
          <Typography variant="caption" color="text.secondary">
            Running for {formatUptime(loopback.uptime_seconds)}
          </Typography>
        )}
      </Box>

      {/* Results Display */}
      {loopback && loopback.tx_count > 0 && (
        <Box>
          {/* Success Rate - Large Display */}
          <Card
            sx={{
              mb: 3,
              bgcolor: status === 'pass' ? 'success.light' : status === 'fail' ? 'error.light' : 'background.paper',
              border: 2,
              borderColor: status === 'pass' ? 'success.main' : status === 'fail' ? 'error.main' : 'divider',
            }}
          >
            <CardContent sx={{ textAlign: 'center', py: 3 }}>
              <Typography variant="h3" fontWeight="bold" sx={{ mb: 1 }}>
                {successRate.toFixed(1)}%
              </Typography>
              <Typography variant="h6" color="text.secondary">
                SUCCESS RATE
              </Typography>
            </CardContent>
          </Card>

          {/* Metrics Grid */}
          <Grid container spacing={2} sx={{ mb: 3 }}>
            <Grid item xs={12} sm={4}>
              <Card>
                <CardContent sx={{ textAlign: 'center' }}>
                  <Typography variant="h4" fontWeight="bold" color="primary">
                    {loopback.tx_count}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Sent
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
            <Grid item xs={12} sm={4}>
              <Card>
                <CardContent sx={{ textAlign: 'center' }}>
                  <Typography variant="h4" fontWeight="bold" color="success.main">
                    {loopback.rx_count}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Received
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
            <Grid item xs={12} sm={4}>
              <Card>
                <CardContent sx={{ textAlign: 'center' }}>
                  <Typography variant="h4" fontWeight="bold" color="error.main">
                    {loopback.error_count}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Errors
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
          </Grid>

          {/* Last Test Details */}
          <Box sx={{ bgcolor: 'background.paper', p: 2, borderRadius: 1 }}>
            <Typography variant="caption" color="text.secondary">
              Last Test Sent:
            </Typography>
            <Typography sx={{ fontFamily: 'monospace', mb: 1, wordBreak: 'break-all' }}>
              {loopback.last_test || '(none)'}
            </Typography>
            <Typography variant="caption" color="text.secondary">
              Last Received:
            </Typography>
            <Typography sx={{ fontFamily: 'monospace', wordBreak: 'break-all' }}>
              {loopback.last_received || '(none)'}
            </Typography>
            {loopback.last_test && loopback.last_received && (
              <Box sx={{ mt: 1 }}>
                <Chip
                  label={loopback.last_test === loopback.last_received ? '✓ Match' : '✗ Mismatch'}
                  color={loopback.last_test === loopback.last_received ? 'success' : 'error'}
                  size="small"
                />
              </Box>
            )}
          </Box>

          {/* Troubleshooting */}
          {status === 'fail' && (
            <Alert severity="warning" sx={{ mt: 2 }}>
              <Typography variant="subtitle2" gutterBottom>
                Test Failing - Troubleshooting Steps:
              </Typography>
              <Typography variant="body2" component="div">
                • Verify jumper wire is securely connected to both GPIO17 and GPIO18
                <br />
                • Check that no other device is connected to GPIO17 or GPIO18
                <br />
                • Inspect GPIO pins for damage or poor contact
                <br />• Try a different jumper wire
              </Typography>
            </Alert>
          )}
        </Box>
      )}

      {/* Initial State Message */}
      {(!loopback || loopback.tx_count === 0) && !isRunning && (
        <Box sx={{ textAlign: 'center', py: 4 }}>
          <Typography variant="body1" color="text.secondary">
            Connect GPIO17 to GPIO18 and click &quot;Start Test&quot; to begin
          </Typography>
        </Box>
      )}
    </SectionContent>
  );
};

export default LoopbackTest;
