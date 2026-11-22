import { FC, ReactNode, useCallback, useContext, useEffect, useState } from 'react';

import { Box, Button, Chip, Grid, InputAdornment, MenuItem, Paper, TextField, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { MessageBox, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import * as WSoftApi from './api';
import {
  digitsText, divisionCount, divisionKey, loadcellDivisions, loadcellTestWeight, LoadcellUnit, maxFromKgText, maxToKgText
} from './calibrate';
import { boardErrorText } from './errors';
import { WSoftLoadcell, WSoftLoadcellRequest, WSoftScaleSetup, WSoftStatus } from './types';

// Logged with the admin's name on the board, like every scale setup change.
const SETUP_REASON = 'Load cell scale setup (Calibrate tab)';

interface SetupForm {
  unit: LoadcellUnit;
  division: string;
  max: string;
}

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

// Calibrate tab for a load cell on an HX711 board (Rhyno, 22 Nov 2026). On RS232 and RS485 the
// scale's own device holds its setup; here the board is the scale, so the page sets it up
// first (unit, division size, Max capacity) and then calibrates: zero with the scale empty,
// then a known test weight. The raw count is shown so the wiring can be checked before
// anything is calibrated: it moves when the load cell is pressed.
const WSoftLoadcellCalibrate: FC<{ status: WSoftStatus }> = ({ status }) => {
  const { me } = useContext(AuthenticationContext);
  const [cell, setCell] = useState<WSoftLoadcell>();
  const [setup, setSetup] = useState<WSoftScaleSetup>();
  const [form, setForm] = useState<SetupForm>();
  const [dirty, setDirty] = useState(false);
  const [weightText, setWeightText] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [done, setDone] = useState<string>();

  const loadCell = useCallback(async () => {
    try {
      setCell((await WSoftApi.readWSoftLoadcell()).data);
    } catch (e) {
      setError(boardErrorText(e, 'Could not read the load cell.'));
    }
  }, []);

  const loadSetup = useCallback(async () => {
    try {
      setSetup((await WSoftApi.readWSoftScaleSetup()).data);
    } catch (e) {
      setError(boardErrorText(e, 'Could not read the scale setup.'));
    }
  }, []);

  // The raw count is not in the live status, so ask for it once a second.
  useEffect(() => {
    loadCell();
    loadSetup();
    const timer = setInterval(loadCell, 1000);
    return () => clearInterval(timer);
  }, [loadCell, loadSetup]);

  // The setup boxes follow the board until someone types in them.
  useEffect(() => {
    if (cell && setup && !dirty) {
      setForm({
        unit: cell.unit,
        division: divisionKey(cell.decimals, cell.step),
        max: maxFromKgText(setup.board.max_kg, cell.unit)
      });
    }
  }, [cell, setup, dirty]);

  const run = async (work: () => Promise<void>, doneText: string, failText: string) => {
    setBusy(true);
    setError(undefined);
    setDone(undefined);
    try {
      await work();
      setDone(doneText);
    } catch (e) {
      setError(boardErrorText(e, failText));
    } finally {
      setBusy(false);
    }
  };

  const send = (request: WSoftLoadcellRequest, doneText: string) =>
    run(async () => setCell((await WSoftApi.updateWSoftLoadcell(request)).data), doneText, 'The board did not take it.');

  const approval = status.approval_on || !!cell?.approval_on;
  const locked = approval || !me?.admin;
  const blocked = !cell || !cell.present || locked;

  const divisions = form ? loadcellDivisions(form.unit) : [];
  const division = divisions.find((d) => d.key === form?.division);
  const maxKg = form ? maxToKgText(form.max, form.unit) : undefined;
  const count = form && division ? divisionCount(form.max, form.unit, division) : undefined;
  const maxProblem = !form || form.max.trim() === ''
    ? undefined
    : maxKg === undefined
      ? 'A number above 0, like 6 or 150'
      : count === undefined
        ? 'Max must be a whole number of divisions, and more than one'
        : undefined;

  const change = (next: Partial<SetupForm>) => {
    if (!form) return;
    const merged = { ...form, ...next };
    // kg and g have different lists: keep the choice when it is in both, else start at 1.
    if (next.unit && !loadcellDivisions(next.unit).some((d) => d.key === merged.division)) merged.division = divisionKey(0, 1);
    setForm(merged);
    setDirty(true);
  };

  // The division first, so the board checks Max against the new one.
  const saveSetup = () =>
    run(
      async () => {
        if (!form || !division || maxKg === undefined) return;
        const { decimals, step } = division;
        setCell((await WSoftApi.updateWSoftLoadcell({ action: 'format', decimals, step, unit: form.unit })).data);
        setSetup((await WSoftApi.updateWSoftScaleSetup({ board: { max_kg: maxKg }, reason: SETUP_REASON })).data);
        setDirty(false);
      },
      'Scale setup saved.',
      'Could not save the scale setup.'
    );

  const weight = loadcellTestWeight(weightText);
  const unit = cell?.unit ?? 'kg';

  return (
    <SectionContent title="Calibrate" titleGutter>
      <Paper variant="outlined" sx={{ p: 3, mb: 2, textAlign: 'center', bgcolor: 'action.hover' }}>
        <Typography variant="h2" component="div" sx={{ fontVariantNumeric: 'tabular-nums', fontWeight: 500 }}>
          {cell && cell.weight_digits !== null ? `${digitsText(cell.weight_digits, cell.decimals)} ${cell.unit}` : '----'}
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
          {setup && setup.board.max_kg !== '' && cell && (
            <Chip size="small" variant="outlined" label={`Max ${maxFromKgText(setup.board.max_kg, cell.unit)} ${cell.unit}`} />
          )}
          <Chip size="small" variant="outlined" label={!cell ? 'Reading...' : `Raw count ${cell.raw !== null ? cell.raw : 'none'}`} />
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
          message="Press on the load cell: the raw count should move. If it does, the wiring is good. Then do steps 1 to 3."
          my={2}
        />
      )}
      {approval && (
        <MessageBox level="warning" message="Approval is ON. Switch Approval off on the Live tab before changing anything here." my={2} />
      )}
      {!approval && !me?.admin && <MessageBox level="info" message="Only an admin can change the scale." my={2} />}
      {error && <MessageBox level="error" message={error} my={2} />}
      {done && <MessageBox level="success" message={done} my={2} />}

      <Step number="1" title="Scale setup">
        <Typography paragraph color="text.secondary">
          What this scale is: its unit, the smallest step it shows, and the most it may weigh.
        </Typography>
        <Grid container columnSpacing={2} rowSpacing={1}>
          <Grid item xs={12} sm={3}>
            <TextField
              id="loadcell-unit"
              select
              fullWidth
              label="Unit"
              value={form?.unit ?? 'kg'}
              disabled={busy || locked || !form}
              onChange={(e) => change({ unit: e.target.value as LoadcellUnit })}
            >
              <MenuItem value="kg">kg</MenuItem>
              <MenuItem value="g">g</MenuItem>
            </TextField>
          </Grid>
          <Grid item xs={12} sm={4}>
            <TextField
              id="loadcell-division"
              select
              fullWidth
              label="Division size"
              value={division ? division.key : ''}
              disabled={busy || locked || !form}
              onChange={(e) => change({ division: e.target.value })}
              helperText="The steps the weight moves in"
            >
              {divisions.map((d) => (<MenuItem key={d.key} value={d.key}>{d.text} {form?.unit}</MenuItem>))}
            </TextField>
          </Grid>
          <Grid item xs={12} sm={5}>
            <TextField
              id="loadcell-max"
              fullWidth
              label="Max capacity"
              value={form?.max ?? ''}
              disabled={busy || locked || !form}
              onChange={(e) => change({ max: e.target.value })}
              inputProps={{ inputMode: 'decimal' }}
              InputProps={{ endAdornment: <InputAdornment position="end">{form?.unit ?? 'kg'}</InputAdornment> }}
              error={!!maxProblem}
              helperText={maxProblem ?? (count !== undefined
                ? `${count} divisions. Above Max the screen shows ----`
                : 'The most the scale may weigh. Above it the screen shows ----')}
            />
          </Grid>
        </Grid>
        <Box display="flex" gap={1} alignItems="center" flexWrap="wrap" mt={2}>
          <Button
            variant="contained"
            startIcon={<SaveIcon />}
            disabled={busy || locked || !dirty || !division || !!maxProblem}
            onClick={saveSetup}
          >
            Save setup
          </Button>
          {dirty && (<Button variant="outlined" color="secondary" disabled={busy} onClick={() => setDirty(false)}>Undo</Button>)}
          <Typography variant="body2" color="text.secondary">
            Min weight, tare and stable samples are on the Scale setup tab.
          </Typography>
        </Box>
      </Step>

      <Step number="2" title="Zero">
        <Typography paragraph>Take everything off the scale. Wait a few seconds.</Typography>
        <Button variant="contained" disabled={busy || blocked} onClick={() => send({ action: 'zero' }, 'Zero is set.')}>
          Set zero
        </Button>
      </Step>

      <Step number="3" title="Test weight">
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
            disabled={busy || blocked || dirty || !cell?.zero_set || weight === undefined}
            onClick={() => weight !== undefined && send({ action: 'span', weight, unit }, `Test weight set to ${weight} ${unit}.`)}
          >
            Set test weight
          </Button>
        </Box>
        {cell && !cell.zero_set && (
          <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>Set zero first.</Typography>
        )}
        {dirty && (
          <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>Save the scale setup first.</Typography>
        )}
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Check: the big number should now show the test weight. Take it off and it should go back to 0. If not, do
          steps 2 and 3 again.
        </Typography>
      </Step>
    </SectionContent>
  );
};

export default WSoftLoadcellCalibrate;
