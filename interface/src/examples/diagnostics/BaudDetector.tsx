import React, { FC } from 'react';
import {
  Typography,
  Box,
  Button,
  Alert,
  LinearProgress,
  Card,
  CardContent,
  Stepper,
  Step,
  StepLabel,
} from '@mui/material';
import { SectionContent } from '../../components';
import { useWs } from '../../utils';
import { DiagnosticsData } from '../../types/diagnostics';
import { updateDiagnostics } from '../../api/diagnostics';
import { DIAGNOSTICS_WEBSOCKET_URL } from './LoopbackTest';
import SearchIcon from '@mui/icons-material/Search';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import ErrorIcon from '@mui/icons-material/Error';

const BAUD_RATES = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200];

const BaudDetector: FC = () => {
  const { data, connected } = useWs<DiagnosticsData>(DIAGNOSTICS_WEBSOCKET_URL);

  const baudScan = data?.baud_scan;
  const isScanning = baudScan?.enabled ?? false;
  const status = baudScan?.status ?? 'idle';
  const currentIndex = baudScan?.current_index ?? 0;
  const detectedBaud = baudScan?.detected_baud ?? 0;

  const handleStartScan = () => {
    updateDiagnostics({ baud_scan_enabled: true });
  };

  const handleStopScan = () => {
    updateDiagnostics({ baud_scan_enabled: false });
  };

  const progress = BAUD_RATES.length > 0 ? (currentIndex / BAUD_RATES.length) * 100 : 0;

  return (
    <SectionContent title="Baud Rate Detection" titleGutter>
      {!connected && (
        <Alert severity="warning" sx={{ mb: 2 }}>
          Connecting to diagnostics service...
        </Alert>
      )}

      {/* Instructions */}
      <Alert severity="info" sx={{ mb: 3 }}>
        <Typography variant="subtitle2" gutterBottom>
          Automatic Baud Rate Detection
        </Typography>
        <Typography variant="body2" paragraph sx={{ mb: 1 }}>
          This test automatically detects your scale&apos;s baud rate by testing
          common values. The scale must be actively transmitting data during the scan.
        </Typography>
        <Typography variant="body2">
          <strong>Setup Options:</strong>
          <br />
          • <strong>Option 1:</strong> Connect GPIO17-18 (loopback) to test detection
          <br />• <strong>Option 2:</strong> Connect scale TX to GPIO18 (RX) - must be transmitting
        </Typography>
      </Alert>

      {/* Scan Controls */}
      <Box sx={{ textAlign: 'center', my: 3 }}>
        {!isScanning ? (
          <Button
            variant="contained"
            size="large"
            color="primary"
            startIcon={<SearchIcon />}
            onClick={handleStartScan}
            disabled={!connected}
            sx={{ px: 4, py: 1.5 }}
          >
            Start Scan
          </Button>
        ) : (
          <Button
            variant="outlined"
            size="large"
            color="error"
            onClick={handleStopScan}
            sx={{ px: 4, py: 1.5 }}
          >
            Stop Scan
          </Button>
        )}
      </Box>

      {/* Scanning Progress */}
      {isScanning && status === 'scanning' && (
        <Box sx={{ mb: 3 }}>
          <Box sx={{ mb: 2 }}>
            <Typography variant="body1" color="text.secondary" gutterBottom>
              Scanning... {Math.round(progress)}%
            </Typography>
            <LinearProgress variant="determinate" value={progress} sx={{ height: 8, borderRadius: 1 }} />
          </Box>

          <Typography variant="body2" color="text.secondary" gutterBottom>
            Testing baud rates:
          </Typography>
          <Stepper activeStep={currentIndex} alternativeLabel sx={{ mt: 2 }}>
            {BAUD_RATES.map((rate, index) => (
              <Step key={rate} completed={index < currentIndex}>
                <StepLabel>{rate}</StepLabel>
              </Step>
            ))}
          </Stepper>
        </Box>
      )}

      {/* Detection Result - Found */}
      {status === 'found' && detectedBaud > 0 && (
        <Card
          sx={{
            bgcolor: 'success.light',
            border: 2,
            borderColor: 'success.main',
            mb: 3,
          }}
        >
          <CardContent sx={{ textAlign: 'center', py: 4 }}>
            <CheckCircleIcon sx={{ fontSize: 64, color: 'success.main', mb: 2 }} />
            <Typography variant="h4" fontWeight="bold" sx={{ mb: 1 }}>
              DETECTED: {detectedBaud} baud
            </Typography>
            <Typography variant="body1" color="text.secondary" sx={{ mb: 3 }}>
              Successfully detected baud rate
            </Typography>
            <Button
              variant="contained"
              color="primary"
              onClick={() => {
                // Navigate to Serial Config with detected baud
                window.location.href = '/serial/configuration';
              }}
            >
              Apply to Serial Config
            </Button>
          </CardContent>
        </Card>
      )}

      {/* Detection Result - Not Found */}
      {status === 'not_found' && (
        <Card
          sx={{
            bgcolor: 'warning.light',
            border: 2,
            borderColor: 'warning.main',
            mb: 3,
          }}
        >
          <CardContent sx={{ textAlign: 'center', py: 4 }}>
            <ErrorIcon sx={{ fontSize: 64, color: 'warning.main', mb: 2 }} />
            <Typography variant="h5" fontWeight="bold" sx={{ mb: 1 }}>
              Baud Rate Not Detected
            </Typography>
            <Typography variant="body1" color="text.secondary" sx={{ mb: 2 }}>
              No data received at any tested baud rate
            </Typography>
            <Alert severity="warning" sx={{ textAlign: 'left', mt: 2 }}>
              <Typography variant="subtitle2" gutterBottom>
                Troubleshooting Steps:
              </Typography>
              <Typography variant="body2" component="div">
                • Verify scale is powered on and transmitting data
                <br />
                • Check wiring: scale TX → GPIO18 (RX), GND → GND
                <br />
                • Ensure scale uses a standard baud rate (1200-115200)
                <br />
                • Try manually setting baud rate in Serial Config
                <br />• Run loopback test first to verify ESP32 hardware
              </Typography>
            </Alert>
          </CardContent>
        </Card>
      )}

      {/* Idle State */}
      {status === 'idle' && !isScanning && (
        <Box sx={{ textAlign: 'center', py: 4 }}>
          <Typography variant="body1" color="text.secondary" paragraph>
            Connect your scale (must be transmitting) and click &quot;Start Scan&quot;
          </Typography>
          <Typography variant="caption" color="text.secondary">
            Scan tests: {BAUD_RATES.join(', ')} baud
          </Typography>
        </Box>
      )}

      {/* Tested Baud Rates Reference */}
      {status === 'idle' && (
        <Box sx={{ mt: 4, p: 2, bgcolor: 'background.paper', borderRadius: 1 }}>
          <Typography variant="caption" color="text.secondary" gutterBottom>
            Tested Baud Rates:
          </Typography>
          <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 1, mt: 1 }}>
            {BAUD_RATES.map((rate) => (
              <Box
                key={rate}
                sx={{
                  px: 1.5,
                  py: 0.5,
                  bgcolor: 'action.hover',
                  borderRadius: 1,
                  fontFamily: 'monospace',
                  fontSize: '0.875rem',
                }}
              >
                {rate}
              </Box>
            ))}
          </Box>
        </Box>
      )}
    </SectionContent>
  );
};

export default BaudDetector;
