import { FC, useState } from 'react';

import {
  Box, Button, Dialog, DialogActions, DialogContent, DialogTitle, Paper, Step, StepContent, StepLabel, Stepper,
  TextField, Typography
} from '@mui/material';

import { FormLoader, MessageBox, SectionContent } from '../../components';
import { useWs } from '../../utils';

import * as WSoftApi from './api';
import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import {
  adWriteRequest, alarmReadRequest, CALIBRATE_BLOCK_TEXT, calibrateBlock, parseTestWeight, spanDigits
} from './calibrate';
import { boardErrorText, plainError } from './errors';
import { WSoftStatus } from './types';

// Calibrate tab (admin). Brought back from the 2 Nov 2026 Waveshare branch on every W-Soft board
// (Rhyno, 14 Nov 2026). Zero, then span with a test weight, then a check. RS485 junction box only:
// see calibrate.ts for the register and for the boards and states that can not calibrate here.
const WSoftCalibrate: FC = () => {
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);
  const [step, setStep] = useState(0);
  const [weightText, setWeightText] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [confirm, setConfirm] = useState<'zero' | 'span'>();
  const [alarmText, setAlarmText] = useState<string>();

  if (!connected || !data) {
    return (
      <SectionContent title="Calibrate" titleGutter>
        <FormLoader message="Connecting..." />
      </SectionContent>
    );
  }

  const status = data;
  const block = calibrateBlock(status);
  const weight = parseTestWeight(weightText);
  const digits = weight !== undefined ? spanDigits(weight, status.decimals) : 0;

  const readAlarm = async () => {
    try {
      const reply = (await WSoftApi.djb(alarmReadRequest())).data;
      const bits = reply.alarm_bits;
      if (!reply.ok || !bits) {
        setAlarmText(`Could not read the alarm (${plainError(reply.error) ?? 'no answer'}).`);
        return;
      }
      const reasons = [
        bits.adc_failure && 'load cell fault (ADC failure or no load cell)',
        bits.over_range && 'over the range',
        bits.eeprom && 'EEPROM flag',
        bits.parameter_changed && 'a setting was changed',
        bits.software_match && 'software and hardware match flag'
      ].filter(Boolean);
      setAlarmText(reasons.length ? `Junction box alarm: ${reasons.join(', ')}.` : 'No alarm bits set now.');
    } catch (e) {
      setAlarmText(boardErrorText(e, 'Could not read the alarm.'));
    }
  };

  const write = async (value: number, next: number) => {
    setConfirm(undefined);
    setBusy(true);
    setError(undefined);
    try {
      const reply = (await WSoftApi.djb(adWriteRequest(value))).data;
      if (reply.ok) {
        setStep(next);
      } else {
        setError(
          `${plainError(reply.error) ?? 'The junction box did not accept it.'} ` +
            `Sent: ${reply.request_hex || '-'}. Got: ${reply.reply_hex || 'nothing'}.`
        );
      }
    } catch (e) {
      setError(boardErrorText(e, 'The calibration was not sent.'));
    } finally {
      setBusy(false);
    }
  };

  // Writer and RS232 boards have nothing to calibrate here: say so and stop.
  if (block === 'writer' || block === 'rs232') {
    return (
      <SectionContent title="Calibrate" titleGutter>
        <MessageBox level="info" message={CALIBRATE_BLOCK_TEXT[block]} my={2} />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Calibrate" titleGutter>
      <MessageBox
        level="warning"
        message={
          'Calibrating changes how this scale weighs. Use a known test weight. ' +
          'Every calibration adds 1 to the audit counter. A scale used for trade must be re-verified after it.'
        }
        my={2}
      />
      <Paper variant="outlined" sx={{ p: 2, mb: 2, textAlign: 'center' }}>
        <Typography variant="h3" component="div" sx={{ fontVariantNumeric: 'tabular-nums' }}>
          {status.weight_text ?? '----'}
        </Typography>
        <Typography variant="body2" color="text.secondary">
          {status.stable ? 'Stable' : 'Not stable'}
          {status.alarm ? ' · junction box alarm' : ''}
          {status.alarm_allowed ? ' · allowed alarm (monitoring)' : ''}
        </Typography>
        {(status.alarm || status.alarm_allowed) && (
          <Button size="small" onClick={readAlarm} disabled={!status.linked}>
            Why the alarm?
          </Button>
        )}
        {alarmText && <Typography variant="body2">{alarmText}</Typography>}
      </Paper>

      {block && <MessageBox level={block === 'approval' ? 'warning' : 'error'} message={CALIBRATE_BLOCK_TEXT[block]} my={2} />}
      {error && <MessageBox level="error" message={error} my={2} />}

      <Stepper activeStep={step} orientation="vertical">
        <Step>
          <StepLabel>Zero</StepLabel>
          <StepContent>
            <Typography paragraph>Take everything off the scale. Wait until it says Stable.</Typography>
            <Button variant="contained" disabled={busy || !!block} onClick={() => setConfirm('zero')}>
              Set zero
            </Button>
          </StepContent>
        </Step>
        <Step>
          <StepLabel>Span</StepLabel>
          <StepContent>
            <Typography paragraph>Put the test weight on. Wait until it says Stable.</Typography>
            <TextField
              label={`Test weight (${status.unit})`}
              value={weightText}
              onChange={(e) => setWeightText(e.target.value)}
              inputProps={{ inputMode: 'decimal' }}
              helperText={weight !== undefined ? `Sent to the junction box as ${digits}` : 'For example 20'}
              sx={{ mb: 1, display: 'block' }}
            />
            <Box display="flex" gap={1}>
              <Button
                variant="contained"
                disabled={busy || !!block || weight === undefined}
                onClick={() => setConfirm('span')}
              >
                Set span
              </Button>
              <Button onClick={() => setStep(0)} disabled={busy}>
                Back
              </Button>
            </Box>
          </StepContent>
        </Step>
        <Step>
          <StepLabel>Check</StepLabel>
          <StepContent>
            <Typography paragraph>
              The reading above should show {weight !== undefined ? `${weight} ${status.unit}` : 'the test weight'}.
              Take it off: it should go back to 0. If it is wrong, start again.
            </Typography>
            <Button onClick={() => setStep(0)}>Start again</Button>
          </StepContent>
        </Step>
      </Stepper>

      <Dialog open={!!confirm} onClose={() => setConfirm(undefined)}>
        <DialogTitle>{confirm === 'zero' ? 'Set zero now?' : `Set span to ${weight} ${status.unit}?`}</DialogTitle>
        <DialogContent>
          This overwrites the calibration in the junction box. The old calibration can not be brought back.
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setConfirm(undefined)}>Cancel</Button>
          <Button variant="contained" onClick={() => (confirm === 'zero' ? write(0, 1) : write(digits, 2))}>
            Yes, calibrate
          </Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default WSoftCalibrate;
