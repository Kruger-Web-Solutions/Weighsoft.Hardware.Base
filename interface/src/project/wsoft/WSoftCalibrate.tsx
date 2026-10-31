import { FC, useState } from 'react';
import { AxiosError } from 'axios';

import {
  Alert, Box, Button, Dialog, DialogActions, DialogContent, DialogTitle, Paper, Step, StepContent, StepLabel, Stepper,
  TextField, Typography
} from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import * as WSoftApi from './api';
import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import { WSoftDjbResult, WSoftStatus } from './types';

// The DJB-M1X calibrates channel 1 with its AD register (41xxx map): 0 = zero calibration,
// a positive number = load calibration with that weight in display digits (the weight as the
// box shows it, without the decimal point). The firmware refuses writes while approval is on.
const CHANNEL = 1;

const ERROR_TEXT: Record<string, string> = {
  approval_on: 'Approval is ON. Switch it off with the USB cable (APPROVAL OFF) before calibrating.',
  not_linked: 'The ESP can not see the DJB box. Check the RS485 wires.',
  busy: 'The DJB is busy. Try again in a second.',
  ad_range: 'That test weight is out of range.',
  timeout: 'The DJB did not answer. Try again.'
};

/** Test weight as the DJB's display digits, e.g. 2.000 kg with 3 decimals = 2000. */
export const spanDigits = (weight: number, decimals: number) => Math.round(weight * Math.pow(10, decimals));

const djbError = (error: any, fallback: string) => {
  const body = (error as AxiosError<WSoftDjbResult>).response?.data;
  if (body?.error) return ERROR_TEXT[body.error] ?? `The DJB refused it (${body.error}).`;
  return fallback;
};

const WSoftCalibrate: FC = () => {
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);
  const [step, setStep] = useState(0);
  const [weightText, setWeightText] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [confirm, setConfirm] = useState<'zero' | 'span'>();
  const [alarmText, setAlarmText] = useState<string>();

  const readAlarm = async () => {
    try {
      const { data: result } = await WSoftApi.djb({ op: 'read', channel: CHANNEL, field: 'alarm' });
      const bits = result.alarm_bits;
      if (!result.ok || !bits) {
        setAlarmText(`Could not read the alarm (${result.error || 'no answer'}).`);
        return;
      }
      const reasons = [
        bits.adc_failure && 'ADC failure or no load cell',
        bits.over_range && 'over the ADC range',
        bits.eeprom && 'EEPROM failure',
        bits.parameter_changed && 'a parameter was changed',
        bits.software_match && 'software and hardware match flag'
      ].filter(Boolean);
      setAlarmText(reasons.length ? `DJB alarm: ${reasons.join(', ')}.` : 'No alarm bits set now.');
    } catch (e: any) {
      setAlarmText(djbError(e, 'Could not read the alarm.'));
    }
  };

  if (!connected || !data) {
    return (
      <SectionContent title="Calibrate" titleGutter>
        <FormLoader message="Connecting..." />
      </SectionContent>
    );
  }

  const status = data;
  const weight = Number(weightText.replace(',', '.'));
  const weightValid = weightText.trim() !== '' && isFinite(weight) && weight > 0;
  const digits = weightValid ? spanDigits(weight, status.decimals) : 0;

  const write = async (value: number, next: number) => {
    setConfirm(undefined);
    setBusy(true);
    setError(undefined);
    try {
      const response = await WSoftApi.djb({ op: 'write', channel: CHANNEL, field: 'ad', value });
      if (response.data.ok) {
        setStep(next);
      } else {
        const { error: code = '', request_hex, reply_hex } = response.data;
        const text = ERROR_TEXT[code] ?? `The DJB did not accept it (${code || 'no answer'}).`;
        setError(`${text} Sent: ${request_hex || '-'}. Got: ${reply_hex || 'nothing'}.`);
      }
    } catch (e: any) {
      setError(djbError(e, 'The calibration was not sent.'));
    } finally {
      setBusy(false);
    }
  };

  const blocked = status.approval_on || !status.linked;

  return (
    <SectionContent title="Calibrate" titleGutter>
      <Paper variant="outlined" sx={{ p: 2, mb: 2, textAlign: 'center' }}>
        <Typography variant="h3" component="div" sx={{ fontVariantNumeric: 'tabular-nums' }}>
          {status.weight_text ?? '----'}
        </Typography>
        <Typography variant="body2" color="text.secondary">
          {status.stable ? 'Stable' : 'Not stable'}{status.alarm ? ' · DJB alarm' : ''}
        </Typography>
        {status.alarm && (
          <Button size="small" onClick={readAlarm} disabled={!status.linked}>
            Why the alarm?
          </Button>
        )}
        {alarmText && <Typography variant="body2">{alarmText}</Typography>}
      </Paper>

      {status.approval_on && <Alert severity="warning" sx={{ mb: 2 }}>{ERROR_TEXT.approval_on}</Alert>}
      {!status.linked && <Alert severity="error" sx={{ mb: 2 }}>{ERROR_TEXT.not_linked}</Alert>}
      {error && <Alert severity="error" sx={{ mb: 2 }}>{error}</Alert>}

      <Stepper activeStep={step} orientation="vertical">
        <Step>
          <StepLabel>Zero</StepLabel>
          <StepContent>
            <Typography paragraph>Take everything off the scale. Wait until the reading is steady.</Typography>
            <Button variant="contained" disabled={busy || blocked} onClick={() => setConfirm('zero')}>
              Set zero
            </Button>
          </StepContent>
        </Step>
        <Step>
          <StepLabel>Span</StepLabel>
          <StepContent>
            <Typography paragraph>Put the test weight on. Wait until the reading is steady.</Typography>
            <TextField
              label={`Test weight (${status.unit})`}
              value={weightText}
              onChange={(e) => setWeightText(e.target.value)}
              inputProps={{ inputMode: 'decimal' }}
              helperText={weightValid ? `Sent to the DJB as ${digits}` : 'For example 20'}
              sx={{ mb: 1, display: 'block' }}
            />
            <Box display="flex" gap={1}>
              <Button variant="contained" disabled={busy || blocked || !weightValid} onClick={() => setConfirm('span')}>
                Set span
              </Button>
              <Button onClick={() => setStep(0)} disabled={busy}>Back</Button>
            </Box>
          </StepContent>
        </Step>
        <Step>
          <StepLabel>Check</StepLabel>
          <StepContent>
            <Typography paragraph>
              The reading above should show {weightValid ? `${weight} ${status.unit}` : 'the test weight'}.
              Take it off: it should go back to 0. If it is wrong, start again.
            </Typography>
            <Button onClick={() => setStep(0)}>Start again</Button>
          </StepContent>
        </Step>
      </Stepper>

      <Dialog open={!!confirm} onClose={() => setConfirm(undefined)}>
        <DialogTitle>{confirm === 'zero' ? 'Set zero now?' : `Set span to ${weight} ${status.unit}?`}</DialogTitle>
        <DialogContent>
          This overwrites the calibration in the DJB box. The old calibration can not be brought back.
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setConfirm(undefined)}>Cancel</Button>
          <Button
            variant="contained"
            onClick={() => (confirm === 'zero' ? write(0, 1) : write(digits, 2))}
          >
            Yes, calibrate
          </Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default WSoftCalibrate;
