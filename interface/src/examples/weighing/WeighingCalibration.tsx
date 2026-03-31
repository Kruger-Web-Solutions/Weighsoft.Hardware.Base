import React, { FC, useState } from 'react';
import {
  Typography,
  Box,
  Button,
  TextField,
  Alert,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogContentText,
  DialogActions,
  Chip,
  Paper,
  Divider,
} from '@mui/material';
import LockIcon from '@mui/icons-material/Lock';
import LockOpenIcon from '@mui/icons-material/LockOpen';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readWeighingData, updateWeighingData } from '../../api/weighing';
import { WeighingData } from '../../types/weighing';

const WeighingCalibration: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } =
    useRest<WeighingData>({ read: readWeighingData, update: updateWeighingData });

  const [knownWeight, setKnownWeight] = useState('');
  const [pinInput, setPinInput] = useState('');
  const [sealDialogOpen, setSealDialogOpen] = useState(false);
  const [pinError, setPinError] = useState('');
  const [calibrating, setCalibrating] = useState(false);
  const [unsealing, setUnsealing] = useState(false);

  if (!data) {
    return (
      <SectionContent title="Calibration" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const sealed = data.seal_active;
  const locked = data.pin_lockout_seconds > 0;
  const uncalibrated = data.calibration_factor === 1.0;

  const handleCalibrate = async () => {
    const kw = parseFloat(knownWeight);
    if (isNaN(kw) || kw <= 0) return;
    setCalibrating(true);
    try {
      await updateWeighingData({ calibrate: true, known_weight: kw } as any);
    } catch (_) { /* logged on serial */ }
    setKnownWeight('');
    setCalibrating(false);
    loadData();
  };

  const handleSeal = async () => {
    setSealDialogOpen(false);
    try {
      await updateWeighingData({ seal: true } as any);
    } catch (_) { /* logged on serial */ }
    loadData();
  };

  const handleUnseal = async () => {
    if (!pinInput) return;
    setPinError('');
    setUnsealing(true);
    try {
      await updateWeighingData({ unseal: true, pin: pinInput } as any);
    } catch (_) { /* logged on serial */ }
    setUnsealing(false);
    await loadData();
    setPinInput('');
  };

  return (
    <SectionContent title="Calibration" titleGutter>
      {/* Event counter -- always visible */}
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
        <Typography variant="body2" color="text.secondary">Event Counter (OIML 6.2.3.5):</Typography>
        <Chip label={data.event_counter} color="primary" size="small" />
        {sealed && <Chip icon={<LockIcon />} label="SEALED" color="error" size="small" />}
        {!sealed && <Chip icon={<LockOpenIcon />} label="Unsealed" color="success" size="small" />}
      </Box>

      {/* Current calibration values (always read-only) */}
      <Paper elevation={1} sx={{ p: 2, mb: 3, bgcolor: 'background.paper' }}>
        <Typography variant="subtitle2" gutterBottom color="text.primary">
          Current Calibration Values
        </Typography>
        <Typography variant="body2" color="text.secondary">
          Zero offset: <strong>{data.zero_offset}</strong> raw counts
        </Typography>
        <Typography variant="body2" color="text.secondary">
          Calibration factor: <strong>{data.calibration_factor?.toFixed(8)}</strong>
        </Typography>
        <Typography variant="body2" color="text.secondary">
          Last calibration:{' '}
          <strong>{data.last_calibration_time || 'Never'}</strong>
        </Typography>
      </Paper>

      <Divider sx={{ mb: 3 }} />

      {/* === UNSEALED STATE === */}
      {!sealed && (
        <>
          <Alert severity="info" sx={{ mb: 2 }}>
            Calibration procedure: (1) Ensure scale is empty and stable, then press{' '}
            <strong>Zero</strong> in the Live Stream tab. (2) Place known weight and enter value below.
          </Alert>

          <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, mb: 3 }}>
            <Typography variant="h6" gutterBottom>Span Calibration</Typography>
            <TextField
              fullWidth
              type="number"
              label="Known Weight"
              value={knownWeight}
              onChange={(e) => setKnownWeight(e.target.value)}
              disabled={calibrating || saving}
              helperText={`Place this weight on the scale and ensure reading is stable. Unit: ${data.unit}`}
              inputProps={{ step: 0.001, min: 0.001 }}
            />
            <ButtonRow>
              <Button
                variant="contained"
                color="primary"
                disabled={!knownWeight || parseFloat(knownWeight) <= 0 || calibrating || saving || (!uncalibrated && !data.stable)}
                onClick={handleCalibrate}
              >
                {calibrating ? 'Calibrating...' : 'Calibrate'}
              </Button>
            </ButtonRow>
            {!data.stable && !uncalibrated && (
              <Alert severity="warning" sx={{ mt: 1 }}>
                Wait for a stable reading before calibrating.
              </Alert>
            )}
            {uncalibrated && (
              <Alert severity="info" sx={{ mt: 1 }}>
                Initial calibration — stability check bypassed. Zero first, then place known weight.
              </Alert>
            )}
          </Box>

          <Divider sx={{ mb: 3 }} />

          <Typography variant="h6" gutterBottom>Seal Instrument</Typography>
          <Alert severity="warning" sx={{ mb: 2 }}>
            Sealing prevents any calibration or configuration changes. The factory PIN is required
            to unseal. Ensure calibration is complete before sealing.
          </Alert>
          <ButtonRow>
            <Button
              variant="contained"
              color="warning"
              startIcon={<LockIcon />}
              onClick={() => setSealDialogOpen(true)}
              disabled={saving}
            >
              Seal Instrument
            </Button>
          </ButtonRow>

          {/* Seal confirmation dialog */}
          <Dialog open={sealDialogOpen} onClose={() => setSealDialogOpen(false)}>
            <DialogTitle>Seal Instrument?</DialogTitle>
            <DialogContent>
              <DialogContentText>
                This will activate the electronic seal. Configuration and calibration changes will
                be blocked until the factory PIN is used to unseal. The event counter will increment.
              </DialogContentText>
            </DialogContent>
            <DialogActions>
              <Button onClick={() => setSealDialogOpen(false)}>Cancel</Button>
              <Button onClick={handleSeal} color="warning" variant="contained" autoFocus>
                Seal
              </Button>
            </DialogActions>
          </Dialog>
        </>
      )}

      {/* === SEALED STATE === */}
      {sealed && (
        <>
          <Alert severity="error" icon={<LockIcon />} sx={{ mb: 3 }}>
            Instrument is sealed. All calibration and configuration changes are blocked.
            Enter the factory PIN to unseal.
          </Alert>

          <Typography variant="h6" gutterBottom>Unseal Instrument</Typography>
          <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
            {locked && (
              <Alert severity="error">
                Too many failed PIN attempts. Locked for {data.pin_lockout_seconds} seconds.
              </Alert>
            )}
            {pinError && !locked && (
              <Alert severity="error">{pinError}</Alert>
            )}
            <TextField
              fullWidth
              type="password"
              label="Factory PIN"
              value={pinInput}
              onChange={(e) => setPinInput(e.target.value)}
              disabled={unsealing || saving || locked}
              helperText="Enter the factory-programmed calibration PIN"
              inputProps={{ maxLength: 8 }}
            />
            <ButtonRow>
              <Button
                variant="contained"
                color="primary"
                startIcon={<LockOpenIcon />}
                onClick={handleUnseal}
                disabled={!pinInput || unsealing || saving || locked}
              >
                {unsealing ? 'Verifying...' : 'Unseal'}
              </Button>
            </ButtonRow>
          </Box>
        </>
      )}
    </SectionContent>
  );
};

export default WeighingCalibration;
