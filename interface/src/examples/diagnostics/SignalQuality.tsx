import React, { FC, useState } from 'react';
import {
  Typography,
  Box,
  Button,
  Alert,
  Grid,
  Card,
  CardContent,
  LinearProgress,
  MenuItem,
  TextField,
  CircularProgress,
} from '@mui/material';
import { SectionContent } from '../../components';
import { useWs } from '../../utils';
import { DiagnosticsData } from '../../types/diagnostics';
import { updateDiagnostics } from '../../api/diagnostics';
import { DIAGNOSTICS_WEBSOCKET_URL } from './LoopbackTest';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';

const SignalQuality: FC = () => {
  const { data, connected } = useWs<DiagnosticsData>(DIAGNOSTICS_WEBSOCKET_URL);
  const [packetCount, setPacketCount] = useState(100);

  const signal = data?.signal_quality;
  const isRunning = signal?.enabled ?? false;
  const status = signal?.status ?? 'idle';
  const quality = signal?.quality_percent ?? 0;
  const progress = signal?.progress ?? 0;

  const handleStartTest = () => {
    updateDiagnostics({
      signal_test_enabled: true,
      signal_total_packets: packetCount,
    });
  };

  // Quality rating based on percentage
  const getQualityRating = () => {
    if (quality >= 95) return { label: 'EXCELLENT', color: 'success.main' };
    if (quality >= 80) return { label: 'GOOD', color: 'success.light' };
    if (quality >= 60) return { label: 'FAIR', color: 'warning.main' };
    return { label: 'POOR', color: 'error.main' };
  };

  const rating = getQualityRating();

  return (
    <SectionContent title="Signal Quality Test" titleGutter>
      {!connected && (
        <Alert severity="warning" sx={{ mb: 2 }}>
          Connecting to diagnostics service...
        </Alert>
      )}

      {/* Instructions */}
      <Alert severity="info" sx={{ mb: 3 }}>
        <Typography variant="subtitle2" gutterBottom>
          Signal Quality Analysis
        </Typography>
        <Typography variant="body2" paragraph sx={{ mb: 1 }}>
          Measures serial connection reliability and performance by sending test
          packets and calculating success rate, latency, and jitter.
        </Typography>
        <Typography variant="body2">
          <strong>Setup:</strong> Connect GPIO17 to GPIO18 with a jumper wire (loopback)
        </Typography>
      </Alert>

      {/* Test Configuration */}
      {status === 'idle' && (
        <Box sx={{ mb: 3 }}>
          <TextField
            select
            fullWidth
            label="Packet Count"
            value={packetCount}
            onChange={(e) => setPacketCount(Number(e.target.value))}
            disabled={isRunning}
            sx={{ mb: 2 }}
          >
            <MenuItem value={10}>10 packets (~1 second)</MenuItem>
            <MenuItem value={100}>100 packets (~5 seconds)</MenuItem>
            <MenuItem value={250}>250 packets (~13 seconds)</MenuItem>
          </TextField>
          <Typography variant="caption" color="text.secondary">
            More packets = longer test = more accurate results
          </Typography>
        </Box>
      )}

      {/* Test Controls */}
      <Box sx={{ textAlign: 'center', my: 3 }}>
        <Button
          variant="contained"
          size="large"
          color="primary"
          startIcon={<PlayArrowIcon />}
          onClick={handleStartTest}
          disabled={!connected || isRunning}
          sx={{ px: 4, py: 1.5 }}
        >
          Run Quality Test
        </Button>
      </Box>

      {/* Running State */}
      {isRunning && status === 'running' && signal && (
        <Box sx={{ mb: 3 }}>
          <Typography variant="body1" color="text.secondary" gutterBottom>
            Testing... {Math.round(progress)}%
          </Typography>
          <LinearProgress variant="determinate" value={progress} sx={{ height: 8, borderRadius: 1, mb: 2 }} />
          <Typography variant="caption" color="text.secondary">
            Sent: {signal.sent_packets} / {signal.total_packets} packets
          </Typography>
        </Box>
      )}

      {/* Results Display */}
      {status === 'complete' && signal && (
        <Box>
          {/* Quality Meter */}
          <Card
            sx={{
              mb: 3,
              bgcolor: quality >= 80 ? 'success.light' : quality >= 60 ? 'warning.light' : 'error.light',
              border: 3,
              borderColor: rating.color,
            }}
          >
            <CardContent sx={{ textAlign: 'center', py: 4 }}>
              <Box sx={{ position: 'relative', display: 'inline-flex', mb: 2 }}>
                <CircularProgress
                  variant="determinate"
                  value={quality}
                  size={150}
                  thickness={6}
                  sx={{ color: rating.color }}
                />
                <Box
                  sx={{
                    top: 0,
                    left: 0,
                    bottom: 0,
                    right: 0,
                    position: 'absolute',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                  }}
                >
                  <Typography variant="h3" fontWeight="bold">
                    {quality}%
                  </Typography>
                </Box>
              </Box>
              <Typography variant="h5" fontWeight="bold" sx={{ color: rating.color, mb: 1 }}>
                {rating.label}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Signal Quality
              </Typography>
              {quality === 100 && (
                <Box sx={{ mt: 2 }}>
                  <CheckCircleIcon sx={{ fontSize: 32, color: 'success.main' }} />
                </Box>
              )}
            </CardContent>
          </Card>

          {/* Detailed Metrics */}
          <Grid container spacing={2} sx={{ mb: 3 }}>
            <Grid item xs={12} sm={6}>
              <Card>
                <CardContent>
                  <Typography variant="caption" color="text.secondary">
                    Avg Latency
                  </Typography>
                  <Typography variant="h5" fontWeight="bold" color="primary">
                    {signal.avg_latency_ms.toFixed(2)} ms
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
            <Grid item xs={12} sm={6}>
              <Card>
                <CardContent>
                  <Typography variant="caption" color="text.secondary">
                    Jitter
                  </Typography>
                  <Typography variant="h5" fontWeight="bold" color="primary">
                    {signal.jitter_ms.toFixed(2)} ms
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
            <Grid item xs={12} sm={6}>
              <Card>
                <CardContent>
                  <Typography variant="caption" color="text.secondary">
                    Packet Loss
                  </Typography>
                  <Typography variant="h5" fontWeight="bold" color={signal.error_count > 0 ? 'error.main' : 'success.main'}>
                    {((signal.error_count / signal.total_packets) * 100).toFixed(1)}%
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
            <Grid item xs={12} sm={6}>
              <Card>
                <CardContent>
                  <Typography variant="caption" color="text.secondary">
                    Errors
                  </Typography>
                  <Typography variant="h5" fontWeight="bold" color={signal.error_count > 0 ? 'error.main' : 'success.main'}>
                    {signal.error_count} / {signal.total_packets}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
          </Grid>

          {/* Test Summary */}
          <Box sx={{ bgcolor: 'background.paper', p: 2, borderRadius: 1 }}>
            <Typography variant="caption" color="text.secondary" gutterBottom>
              Test Summary
            </Typography>
            <Grid container spacing={1} sx={{ mt: 1 }}>
              <Grid item xs={6}>
                <Typography variant="body2">
                  <strong>Packets Sent:</strong> {signal.sent_packets}
                </Typography>
              </Grid>
              <Grid item xs={6}>
                <Typography variant="body2">
                  <strong>Packets Received:</strong> {signal.received_packets}
                </Typography>
              </Grid>
              <Grid item xs={6}>
                <Typography variant="body2">
                  <strong>Success Rate:</strong> {quality.toFixed(1)}%
                </Typography>
              </Grid>
              <Grid item xs={6}>
                <Typography variant="body2">
                  <strong>Failed:</strong> {signal.error_count}
                </Typography>
              </Grid>
            </Grid>
          </Box>

          {/* Recommendations */}
          {quality < 95 && (
            <Alert severity={quality < 60 ? 'error' : 'warning'} sx={{ mt: 2 }}>
              <Typography variant="subtitle2" gutterBottom>
                {quality < 60 ? 'Poor Signal Quality Detected' : 'Signal Quality Could Be Improved'}
              </Typography>
              <Typography variant="body2" component="div">
                • Check jumper wire connection (ensure firm contact)
                <br />
                • Inspect GPIO pins for damage or corrosion
                <br />
                • Try a different jumper wire (poor wire quality can cause issues)
                <br />
                • Verify no electrical interference nearby
                <br />• Run test again with more packets for better accuracy
              </Typography>
            </Alert>
          )}

          {/* Run Another Test */}
          <Box sx={{ textAlign: 'center', mt: 3 }}>
            <Button
              variant="outlined"
              onClick={() => updateDiagnostics({ signal_test_enabled: false })}
              sx={{ mr: 1 }}
            >
              New Test
            </Button>
          </Box>
        </Box>
      )}

      {/* Idle State */}
      {status === 'idle' && !isRunning && (
        <Box sx={{ textAlign: 'center', py: 4 }}>
          <Typography variant="body1" color="text.secondary">
            Connect GPIO17 to GPIO18 and click &quot;Run Quality Test&quot;
          </Typography>
        </Box>
      )}
    </SectionContent>
  );
};

export default SignalQuality;
