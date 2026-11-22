import { FC, ReactNode, useCallback, useContext, useEffect, useState } from 'react';
import { Link as RouterLink } from 'react-router-dom';

import { Box, Button, Chip, InputAdornment, Link, Paper, TextField, Typography } from '@mui/material';

import { MessageBox, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import * as WSoftApi from './api';
import { digitsText, loadcellTestWeight, noiseText } from './calibrate';
import { boardErrorText } from './errors';
import { WSoftLoadcell, WSoftLoadcellRequest, WSoftStatus } from './types';

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

// Calibrate tab for a load cell on an HX711 board (Rhyno, 22 Nov 2026). The board keeps the
// calibration itself: zero with the scale empty, then a known test weight. The internal (raw) count is
// shown live so the wiring can be checked before anything is calibrated: it moves when the load
// cell is pressed. What the scale is (unit, division, Max, Min ...) is on the Scale setup
// tab, the one place for it on every source.
const WSoftLoadcellCalibrate: FC<{ status: WSoftStatus }> = ({ status }) => {
  const { me } = useContext(AuthenticationContext);
  const [cell, setCell] = useState<WSoftLoadcell>();
  const [weightText, setWeightText] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [done, setDone] = useState<string>();

  const load = useCallback(async () => {
    try {
      setCell((await WSoftApi.readWSoftLoadcell()).data);
    } catch (e) {
      setError(boardErrorText(e, 'Could not read the load cell.'));
    }
  }, []);

  // The internal count is not in the live status, so ask for it about four times a second
  // (Rhyno, 22 Nov 2026: show it in real time). The next ask starts only when the last one is
  // back, so a slow board is never asked twice at once.
  useEffect(() => {
    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;
    const tick = async () => {
      await load();
      if (!stopped) timer = setTimeout(tick, COUNT_REFRESH_MS);
    };
    tick();
    return () => {
      stopped = true;
      clearTimeout(timer);
    };
  }, [load]);

  const send = async (request: WSoftLoadcellRequest, doneText: string) => {
    setBusy(true);
    setError(undefined);
    setDone(undefined);
    try {
      setCell((await WSoftApi.updateWSoftLoadcell(request)).data);
      setDone(doneText);
    } catch (e) {
      setError(boardErrorText(e, 'The board did not take it.'));
    } finally {
      setBusy(false);
    }
  };

  const approval = status.approval_on || !!cell?.approval_on;
  const blocked = !cell || !cell.present || approval || !me?.admin;
  const weight = loadcellTestWeight(weightText);
  const unit = cell?.unit ?? 'kg';

  return (
    <SectionContent title="Calibrate" titleGutter>
      <Paper variant="outlined" sx={{ p: 3, mb: 2, textAlign: 'center', bgcolor: 'action.hover' }}>
        <Typography variant="h2" component="div" sx={{ fontVariantNumeric: 'tabular-nums', fontWeight: 500 }}>
          {cell && cell.weight_digits !== null ? `${digitsText(cell.weight_digits, cell.decimals)} ${cell.unit}` : '----'}
        </Typography>
        <Typography variant="h5" component="div" color="text.secondary" sx={{ fontVariantNumeric: 'tabular-nums', mt: 0.5 }}>
          Internal count: {!cell ? '...' : cell.raw !== null ? cell.raw : 'none'}
          {cell && cell.raw !== null && cell.zero_raw !== null && (
            <Typography component="span" variant="body1" sx={{ ml: 1.5 }}>
              ({cell.raw - cell.zero_raw >= 0 ? '+' : ''}{cell.raw - cell.zero_raw} from zero)
            </Typography>
          )}
        </Typography>
        <Box display="flex" gap={1} justifyContent="center" flexWrap="wrap" mt={1}>
          {cell && (
            <Chip
              size="small"
              color={cell.calibrated ? 'success' : 'warning'}
              label={cell.calibrated ? 'Calibrated' : 'Not calibrated yet'}
            />
          )}
          {cell && (
            <Chip size="small" variant="outlined" label={`Division ${digitsText(cell.step, cell.decimals)} ${cell.unit}`} />
          )}
          {cell && cell.rate > 0 && (
            <Chip size="small" variant="outlined" label={`Chip speed ${cell.rate} a second`} />
          )}
          {cell && cell.zero_track > 0 && (
            <Chip size="small" variant="outlined" label={`Auto zero ${cell.zero_offset >= 0 ? '+' : ''}${cell.zero_offset}`} />
          )}
          {cell && cell.noise_tenths !== null && (
            <Chip
              size="small"
              variant="outlined"
              color={cell.noise_tenths > 28 ? 'warning' : 'default'}
              label={`Noise ${noiseText(cell.noise_tenths)} div`}
            />
          )}
        </Box>
      </Paper>

      {cell && !cell.present && (
        <MessageBox
          level="error"
          message={
            'No reading from the load cell board. Check the four wires on the CN1 plug: ' +
            'VCC to 3.3V, GND to GND, DT to IO22, SCK to IO27.'
          }
          my={2}
        />
      )}
      {cell && cell.present && !cell.calibrated && (
        <MessageBox
          level="info"
          message="Press on the load cell: the internal count should move. If it does, the wiring is good. Then do steps 1 and 2."
          my={2}
        />
      )}
      {approval && (
        <MessageBox level="warning" message="Approval is ON. Switch Approval off on the Live tab before calibrating." my={2} />
      )}
      {error && <MessageBox level="error" message={error} my={2} />}
      {done && <MessageBox level="success" message={done} my={2} />}

      <Typography paragraph color="text.secondary">
        Unit, division size, filter and Max capacity are on the{' '}
        <Link component={RouterLink} to="../setup">Scale setup</Link> tab. Set them first.
      </Typography>

      <Step number="1" title="Zero">
        <Typography paragraph>Take everything off the scale. Wait a few seconds.</Typography>
        <Button variant="contained" disabled={busy || blocked} onClick={() => send({ action: 'zero' }, 'Zero is set.')}>
          Set zero
        </Button>
      </Step>

      <Step number="2" title="Test weight">
        <Typography paragraph>Put a weight you know on the scale. Wait a few seconds. Type what it weighs.</Typography>
        <Box display="flex" gap={1} alignItems="flex-start" flexWrap="wrap">
          <TextField
            id="loadcell-test-weight"
            label="Test weight"
            value={weightText}
            onChange={(e) => setWeightText(e.target.value)}
            inputProps={{ inputMode: 'decimal' }}
            InputProps={{ endAdornment: <InputAdornment position="end">{unit}</InputAdornment> }}
            helperText={cell && cell.test_weight_digits !== null
              ? `Last time: ${digitsText(cell.test_weight_digits, cell.decimals)} ${cell.unit}`
              : 'Like 2 or 2.5'}
          />
          <Button
            variant="contained"
            sx={{ mt: 1 }}
            disabled={busy || blocked || !cell?.zero_set || weight === undefined}
            onClick={() => weight !== undefined && send({ action: 'span', weight, unit }, `Test weight set to ${weight} ${unit}.`)}
          >
            Set test weight
          </Button>
        </Box>
        {cell && !cell.zero_set && (
          <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>Set zero first.</Typography>
        )}
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Check: the big number should now show the test weight. Take it off and it should go back to 0. If not, do
          steps 1 and 2 again.
        </Typography>
      </Step>
    </SectionContent>
  );
};

export default WSoftLoadcellCalibrate;
