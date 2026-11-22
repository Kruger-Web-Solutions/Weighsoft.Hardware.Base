import { FC, ReactNode, useContext, useEffect, useState } from 'react';
import { Link as RouterLink } from 'react-router-dom';

import {
  Box, Button, Chip, Dialog, DialogActions, DialogContent, DialogTitle, InputAdornment, Link, Paper, TextField, Typography
} from '@mui/material';

import { FormLoader, MessageBox, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';
import { useWs } from '../../utils';

import * as WSoftApi from './api';
import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import {
  adWriteRequest, alarmReadRequest, CALIBRATE_BLOCK_TEXT, calibrateBlock, calibratesLoadcell, digitsText, noiseText,
  parseTestWeight, TestWeight
} from './calibrate';
import { boardErrorText, plainError } from './errors';
import { WSoftLoadcell, WSoftStatus } from './types';

const Step: FC<{ number: string; title: string; children: ReactNode }> = ({ number, title, children }) => (
  <Paper variant="outlined" sx={{ p: 2, mb: 2 }}>
    <Box display="flex" alignItems="center" gap={1.5} mb={1.5}>
      <Box
        sx={{
          width: 32,
          height: 32,
          borderRadius: '50%',
          bgcolor: 'primary.main',
          color: 'primary.contrastText',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          fontWeight: 700,
          flexShrink: 0
        }}
      >
        {number}
      </Box>
      <Typography variant="h6" component="h3">{title}</Typography>
    </Box>
    {children}
  </Paper>
);

const COUNT_REFRESH_MS = 250;

// The load cell board's own state. The internal count is not in the live status, so ask for it
// about four times a second (Rhyno, 22 Nov 2026: show it in real time). The next ask starts
// only when the last one is back, so a slow board is never asked twice at once.
const useLoadcell = (on: boolean) => {
  const [cell, setCell] = useState<WSoftLoadcell>();
  const [error, setError] = useState<string>();
  useEffect(() => {
    if (!on) return undefined;
    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;
    const tick = async () => {
      try {
        const next = (await WSoftApi.readWSoftLoadcell()).data;
        if (!stopped) {
          setCell(next);
          setError(undefined);
        }
      } catch (e) {
        if (!stopped) setError(boardErrorText(e, 'Could not read the load cell.'));
      }
      if (!stopped) timer = setTimeout(tick, COUNT_REFRESH_MS);
    };
    tick();
    return () => {
      stopped = true;
      clearTimeout(timer);
    };
  }, [on]);
  return { cell, setCell, error };
};

// Calibrate tab (admin): one page for every scale this board can calibrate (Rhyno, 22 Nov 2026).
// Zero with the scale empty, then a known test weight, then look at the big number. Only the
// last step differs: an RS485 junction box takes the calibration itself (calibrate.ts has the
// register), a load cell on an HX711 board has it kept on this board. A Writer and an RS232
// scale have nothing to calibrate here.
const WSoftCalibrate: FC = () => {
  const { me } = useContext(AuthenticationContext);
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);
  const onCell = !!data && calibratesLoadcell(data);
  const { cell, setCell, error: cellError } = useLoadcell(onCell);
  const [weightText, setWeightText] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [done, setDone] = useState<string>();
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
  const block = calibrateBlock(status, cell?.present);
  // Writer and RS232 boards have nothing to calibrate here: say so and stop.
  if (block === 'writer' || block === 'rs232') {
    return (
      <SectionContent title="Calibrate" titleGutter>
        <MessageBox level="info" message={CALIBRATE_BLOCK_TEXT[block]} my={2} />
      </SectionContent>
    );
  }

  // What the two sources show differently. Everything below this block is the same page.
  const unit = onCell ? cell?.unit ?? 'kg' : status.unit;
  const decimals = onCell ? cell?.decimals ?? status.decimals : status.decimals;
  const reading = onCell
    ? cell && cell.weight_digits !== null ? `${digitsText(cell.weight_digits, cell.decimals)} ${cell.unit}` : '----'
    : status.weight_text !== null && status.weight_text !== undefined ? `${status.weight_text} ${status.unit}` : '----';
  const keptIn = onCell ? 'on this board' : 'in the junction box';
  const zeroFirst = onCell && !!cell && !cell.zero_set;
  const blocked = busy || !!block || !me?.admin || (onCell && !cell);
  const weight = parseTestWeight(weightText, decimals);
  // A load cell that is not calibrated yet shows no weight, so it has no steady or moving to tell.
  const judged = !onCell || !!cell?.calibrated;
  const steady = status.stable;

  // The last step: where the calibration goes. Returns what went wrong, or undefined.
  const sendToJunctionBox = async (digits: number): Promise<string | undefined> => {
    const reply = (await WSoftApi.djb(adWriteRequest(digits))).data;
    if (reply.ok) return undefined;
    return `${plainError(reply.error) ?? 'The junction box did not accept it.'} ` +
      `Sent: ${reply.request_hex || '-'}. Got: ${reply.reply_hex || 'nothing'}.`;
  };
  const keepOnBoard = async (test?: TestWeight): Promise<string | undefined> => {
    const request = test ? { action: 'span' as const, weight: test.text, unit: cell?.unit ?? 'kg' } : { action: 'zero' as const };
    setCell((await WSoftApi.updateWSoftLoadcell(request)).data);
    return undefined;
  };

  const calibrate = async (test?: TestWeight) => {
    setConfirm(undefined);
    setBusy(true);
    setError(undefined);
    setDone(undefined);
    try {
      const failed = onCell ? await keepOnBoard(test) : await sendToJunctionBox(test ? test.digits : 0);
      if (failed) setError(failed);
      else setDone(test ? `Test weight set to ${test.text} ${unit}.` : 'Zero is set.');
    } catch (e) {
      setError(boardErrorText(e, 'The calibration was not taken.'));
    } finally {
      setBusy(false);
    }
  };

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

  return (
    <SectionContent title="Calibrate" titleGutter>
      <Paper variant="outlined" sx={{ p: 3, mb: 2, textAlign: 'center', bgcolor: 'action.hover' }}>
        <Typography variant="h2" component="div" sx={{ fontVariantNumeric: 'tabular-nums', fontWeight: 500 }}>
          {reading}
        </Typography>
        {onCell && (
          <Typography variant="h5" component="div" color="text.secondary" sx={{ fontVariantNumeric: 'tabular-nums', mt: 0.5 }}>
            Internal count: {!cell ? '...' : cell.raw !== null ? cell.raw : 'none'}
            {cell && cell.raw !== null && cell.zero_raw !== null && (
              <Typography component="span" variant="body1" sx={{ ml: 1.5 }}>
                ({cell.raw - cell.zero_raw >= 0 ? '+' : ''}{cell.raw - cell.zero_raw} from zero)
              </Typography>
            )}
          </Typography>
        )}
        <Box display="flex" gap={1} justifyContent="center" flexWrap="wrap" mt={1}>
          {judged && <Chip size="small" color={steady ? 'success' : 'default'} label={steady ? 'Stable' : 'Not stable'} />}
          {onCell && cell && (
            <Chip
              size="small"
              color={cell.calibrated ? 'success' : 'warning'}
              label={cell.calibrated ? 'Calibrated' : 'Not calibrated yet'}
            />
          )}
          {onCell && cell && (
            <Chip size="small" variant="outlined" label={`Division ${digitsText(cell.step, cell.decimals)} ${cell.unit}`} />
          )}
          {onCell && cell && cell.rate > 0 && (
            <Chip size="small" variant="outlined" label={`Chip speed ${cell.rate} a second`} />
          )}
          {onCell && cell && cell.zero_track > 0 && (
            <Chip size="small" variant="outlined" label={`Auto zero ${cell.zero_offset >= 0 ? '+' : ''}${cell.zero_offset}`} />
          )}
          {onCell && cell && cell.noise_tenths !== null && (
            <Chip
              size="small"
              variant="outlined"
              color={cell.noise_tenths > 28 ? 'warning' : 'default'}
              label={`Noise ${noiseText(cell.noise_tenths)} div`}
            />
          )}
          {!onCell && status.alarm && <Chip size="small" color="error" label="Junction box alarm" />}
          {!onCell && status.alarm_allowed && <Chip size="small" color="warning" label="Allowed alarm (monitoring)" />}
        </Box>
        {!onCell && (status.alarm || status.alarm_allowed) && (
          <Button size="small" onClick={readAlarm} disabled={!status.linked} sx={{ mt: 1 }}>
            Why the alarm?
          </Button>
        )}
        {alarmText && <Typography variant="body2">{alarmText}</Typography>}
      </Paper>

      {block && <MessageBox level={block === 'approval' ? 'warning' : 'error'} message={CALIBRATE_BLOCK_TEXT[block]} my={2} />}
      {onCell && cell && cell.present && !cell.calibrated && (
        <MessageBox
          level="info"
          message="Press on the load cell: the internal count should move. If it does, the wiring is good. Then do steps 1 and 2."
          my={2}
        />
      )}
      {(error ?? cellError) && <MessageBox level="error" message={error ?? cellError ?? ''} my={2} />}
      {done && <MessageBox level="success" message={done} my={2} />}

      <Typography paragraph color="text.secondary">
        What the scale is (unit, division size, Max capacity) is on the{' '}
        <Link component={RouterLink} to="../setup">Scale setup</Link> tab. Set it first.
        {!onCell && ' Every calibration adds 1 to the audit counter.'}
      </Typography>

      <Step number="1" title="Zero">
        <Typography paragraph>Take everything off the scale. Wait until it says Stable.</Typography>
        <Button variant="contained" disabled={blocked} onClick={() => setConfirm('zero')}>
          Set zero
        </Button>
      </Step>

      <Step number="2" title="Test weight">
        <Typography paragraph>Put a weight you know on the scale. Wait until it says Stable. Type what it weighs.</Typography>
        <Box display="flex" gap={1} alignItems="flex-start" flexWrap="wrap">
          <TextField
            id="calibrate-test-weight"
            label="Test weight"
            value={weightText}
            onChange={(e) => setWeightText(e.target.value)}
            inputProps={{ inputMode: 'decimal' }}
            InputProps={{ endAdornment: <InputAdornment position="end">{unit}</InputAdornment> }}
            error={weightText.trim() !== '' && !weight}
            helperText={
              weightText.trim() !== '' && !weight
                ? `A number above 0, at most ${decimals} digits after the point`
                : onCell && cell && cell.test_weight_digits !== null
                  ? `Last time: ${digitsText(cell.test_weight_digits, cell.decimals)} ${cell.unit}`
                  : 'Like 2 or 2.5'
            }
          />
          <Button variant="contained" sx={{ mt: 1 }} disabled={blocked || zeroFirst || !weight} onClick={() => setConfirm('span')}>
            Set test weight
          </Button>
        </Box>
        {zeroFirst && (
          <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>Set zero first.</Typography>
        )}
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Check: the big number should now show the test weight. Take it off and it should go back to 0. If not, do
          steps 1 and 2 again.
        </Typography>
      </Step>

      <Dialog open={!!confirm} onClose={() => setConfirm(undefined)}>
        <DialogTitle>
          {confirm === 'zero' ? 'Set zero now?' : `Set the test weight to ${weight?.text ?? ''} ${unit}?`}
        </DialogTitle>
        <DialogContent>
          {judged && !steady && (
            <MessageBox level="warning" message="The weight is still moving. It is better to wait until it says Stable." mb={2} />
          )}
          This replaces the calibration kept {keptIn}. The old calibration can not be brought back.
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setConfirm(undefined)}>Cancel</Button>
          <Button
            variant="contained"
            disabled={confirm === 'span' && !weight}
            onClick={() => calibrate(confirm === 'span' ? weight : undefined)}
          >
            Yes, calibrate
          </Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default WSoftCalibrate;
