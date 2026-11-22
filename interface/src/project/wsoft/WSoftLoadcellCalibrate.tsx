import { FC, useCallback, useEffect, useState } from 'react';

import { Box, Button, MenuItem, Paper, TextField, Typography } from '@mui/material';

import { MessageBox, SectionContent } from '../../components';

import * as WSoftApi from './api';
import { digitsText, loadcellTestWeight } from './calibrate';
import { boardErrorText } from './errors';
import { WSoftLoadcell, WSoftLoadcellRequest, WSoftStatus } from './types';

const STEPS = [1, 2, 5, 10, 20, 50];

// Calibrate tab for a load cell on an HX711 board (Rhyno, 22 Nov 2026). The board keeps the
// calibration itself: zero with the scale empty, then a known test weight. The raw count is
// shown so the wiring can be checked before anything is calibrated: it moves when the load
// cell is pressed.
const WSoftLoadcellCalibrate: FC<{ status: WSoftStatus }> = ({ status }) => {
  const [cell, setCell] = useState<WSoftLoadcell>();
  const [weightText, setWeightText] = useState('');
  const [unit, setUnit] = useState<'kg' | 'g'>('kg');
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

  // The raw count is not in the live status, so ask for it once a second.
  useEffect(() => {
    load();
    const timer = setInterval(load, 1000);
    return () => clearInterval(timer);
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

  const weight = loadcellTestWeight(weightText);
  const blocked = !cell || !cell.present || cell.approval_on || status.approval_on;

  return (
    <SectionContent title="Calibrate" titleGutter>
      <Paper variant="outlined" sx={{ p: 2, mb: 2, textAlign: 'center' }}>
        <Typography variant="h3" component="div" sx={{ fontVariantNumeric: 'tabular-nums' }}>
          {cell && cell.weight_digits !== null ? `${digitsText(cell.weight_digits, cell.decimals)} ${cell.unit}` : '----'}
        </Typography>
        <Typography variant="body2" color="text.secondary">
          {!cell ? 'Reading...' : cell.calibrated ? 'Calibrated' : 'Not calibrated yet'}
          {cell ? ` · raw count ${cell.raw !== null ? cell.raw : 'none'}` : ''}
        </Typography>
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
          message="Press on the load cell: the raw count should move. If it does, the wiring is good. Then do steps 1 and 2."
          my={2}
        />
      )}
      {(status.approval_on || cell?.approval_on) && (
        <MessageBox level="warning" message="Approval is ON. Switch Approval off on the Live tab before calibrating." my={2} />
      )}
      {error && <MessageBox level="error" message={error} my={2} />}
      {done && <MessageBox level="success" message={done} my={2} />}

      <Typography variant="h6" gutterBottom>1. Zero</Typography>
      <Typography paragraph>Take everything off the scale. Wait a few seconds.</Typography>
      <Button variant="contained" disabled={busy || blocked} onClick={() => send({ action: 'zero' }, 'Zero is set.')}>
        Set zero
      </Button>

      <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>2. Test weight</Typography>
      <Typography paragraph>Put a weight you know on the scale. Wait a few seconds. Type what it weighs.</Typography>
      <Box display="flex" gap={1} alignItems="flex-start" flexWrap="wrap">
        <TextField
          id="loadcell-test-weight"
          label="Test weight"
          value={weightText}
          onChange={(e) => setWeightText(e.target.value)}
          inputProps={{ inputMode: 'decimal' }}
          helperText="The digits after the point set what the scale shows: 2.000 shows 3 digits."
        />
        <TextField id="loadcell-unit" select label="Unit" value={unit} onChange={(e) => setUnit(e.target.value as 'kg' | 'g')}>
          <MenuItem value="kg">kg</MenuItem>
          <MenuItem value="g">g</MenuItem>
        </TextField>
      </Box>
      <Button
        variant="contained"
        sx={{ mt: 1 }}
        disabled={busy || blocked || !cell?.zero_set || weight === undefined}
        onClick={() => weight !== undefined && send({ action: 'span', weight, unit }, `Test weight set to ${weight} ${unit}.`)}
      >
        Set test weight
      </Button>
      {cell && !cell.zero_set && (
        <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>Set zero first.</Typography>
      )}

      <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>3. Check</Typography>
      <Typography paragraph>
        The big number should now show the test weight. Take it off: it should go back to 0. If not, do steps 1 and 2 again.
      </Typography>

      <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>Steps the weight moves in</Typography>
      <TextField
        id="loadcell-step"
        select
        label="Step (last digits)"
        value={cell?.step ?? 1}
        disabled={busy || !cell || cell.approval_on || status.approval_on}
        onChange={(e) => send({ action: 'step', step: Number(e.target.value) }, 'Step saved.')}
        helperText="A bigger step makes a jumpy last digit calmer."
        sx={{ minWidth: 200 }}
      >
        {STEPS.map((s) => (<MenuItem key={s} value={s}>{s}</MenuItem>))}
      </TextField>
    </SectionContent>
  );
};

export default WSoftLoadcellCalibrate;
