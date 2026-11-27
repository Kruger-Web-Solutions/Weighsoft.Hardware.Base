import { FC, useCallback, useContext, useEffect, useRef, useState } from 'react';
import { useSnackbar } from 'notistack';

import { Box, Button, Checkbox, Chip, FormControlLabel, Grid, MenuItem, TextField, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { ButtonRow, FormLoader, MessageBox, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import WSoftRs485Check from './WSoftRs485Check';
import * as WSoftApi from './api';
import {
  divisionKey, LOADCELL_AUTO_ZERO, LOADCELL_DRIFT_GUARD, LOADCELL_FILTERS, loadcellDivisions, LoadcellUnit, loadcellUnit, noiseHint
} from './calibrate';
import { boardErrorText, plainError } from './errors';
import {
  boardValues, CardValues, cardWrites, CHECK_ROWS, eStep, FILTER1_MAX, FILTER2_MAX, formTuning, Rs485Form, rs485Problems, toRs485Form,
  UNITS
} from './rs485Setup';
import { WSoftCardField, WSoftLoadcell, WSoftScaleBoard, WSoftScaleSetup as WSoftScaleSetupState } from './types';
import { REASON_MAX, reasonProblem } from './validators';

const REFRESH_MS = 5000;

// The form keeps every value as typed text; it becomes A5 board JSON on Save.
type BoardForm = Record<Exclude<keyof WSoftScaleBoard, 'tare_enabled'>, string> & { tare_enabled: boolean };

const toForm = (board: WSoftScaleBoard): BoardForm => ({
  ...board,
  blank_below_e: board.blank_below_e === null ? '' : String(board.blank_below_e),
  blank_above_e: String(board.blank_above_e),
  stable_samples: String(board.stable_samples)
});

// "0.2 kg" and "0,2" are read as 0.2.
const kgText = (value: string) => value.trim().replace(/\s*kg$/i, '').replace(',', '.');

const toBoard = (form: BoardForm): WSoftScaleBoard => ({
  ...form,
  sa_number: form.sa_number.trim(),
  max_kg: kgText(form.max_kg),
  min_kg: kgText(form.min_kg),
  tare_max_kg: kgText(form.tare_max_kg),
  preset_tare_kg: kgText(form.preset_tare_kg),
  blank_below_e: form.blank_below_e.trim() === '' ? null : Number(form.blank_below_e),
  blank_above_e: Number(form.blank_above_e),
  stable_samples: Number(form.stable_samples)
});

// Quick shape checks only. The board checks the rest (order, steps of e, ranges) and answers with a code.
const KG = /^[0-9]+(\.[0-9]{1,5})?$/;
const WHOLE = /^[0-9]+$/;

const kgProblem = (value: string) => (kgText(value) === '' || KG.test(kgText(value)) ? undefined : 'A number in kg, like 60 or 0.02');

const wholeProblem = (value: string, required: boolean) => {
  if (value.trim() === '') return required ? 'Needed' : undefined;
  return WHOLE.test(value.trim()) ? undefined : 'A whole number';
};

const MISSING_LABEL: Record<string, string> = {
  sa_number: 'SA number',
  max_kg: 'Max',
  min_kg: 'Min',
  blank_below_e: 'Blank below zero',
  blank_above_e: 'Blank above Max',
  stable_samples: 'Stable samples',
  division: 'Card step (e)',
  decimal: 'Card decimal places',
  card: 'Decimal places and e'
};

const WSoftScaleSetup: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const { me } = useContext(AuthenticationContext);
  const isAdmin = !!me?.admin;
  const [setup, setSetup] = useState<WSoftScaleSetupState>();
  const [loadError, setLoadError] = useState<string>();
  const [form, setForm] = useState<BoardForm>();
  // RS485 card: the one form (unit, e, stable, auto zero, filters) and what the card answered.
  const [rsForm, setRsForm] = useState<Rs485Form>(toRs485Form(null, undefined, {}));
  const [cardValues, setCardValues] = useState<CardValues>({});
  const [cardBusy, setCardBusy] = useState(false);
  const filtersAsked = useRef(false);
  // Load cell: the board holds the unit and the division the card holds on RS485, and the filter.
  const [cell, setCell] = useState<WSoftLoadcell>();
  const [cellForm, setCellForm] = useState<{ unit: LoadcellUnit; division: string; filter: number; zeroTrack: number; driftGuard: number }>(
    { unit: 'kg', division: '', filter: 2, zeroTrack: 0, driftGuard: 0 }
  );
  const [dirty, setDirty] = useState(false);
  const [reason, setReason] = useState('');
  const [saving, setSaving] = useState(false);

  const load = useCallback(async () => {
    try {
      const next = (await WSoftApi.readWSoftScaleSetup()).data;
      // The filter and the live noise come from the load cell itself.
      if (next.source === 'loadcell') setCell((await WSoftApi.readWSoftLoadcell()).data);
      setSetup(next);
      setLoadError(undefined);
    } catch (error) {
      setLoadError(boardErrorText(error, 'Could not read the scale setup'));
    }
  }, []);

  useEffect(() => {
    load();
    const timer = setInterval(load, REFRESH_MS);
    return () => clearInterval(timer);
  }, [load]);

  // Follow the board while nothing is being typed.
  useEffect(() => {
    if (setup && !dirty) {
      setForm(toForm(setup.board));
      setRsForm(toRs485Form(setup.card, setup.tuning, cardValues));
      if (setup.card) {
        setCellForm({
          unit: loadcellUnit(setup.card.unit),
          division: divisionKey(setup.card.decimal, setup.card.division),
          filter: cell ? cell.filter : 2,
          zeroTrack: cell ? cell.zero_track : 0,
          driftGuard: cell ? cell.drift_guard : 0
        });
      }
    }
  }, [setup, cell, dirty, cardValues]);

  /** Reads these card settings one by one; false (and a message) when the card does not answer. */
  const readCard = useCallback(async (fields: WSoftCardField[], quiet = false) => {
    setCardBusy(true);
    try {
      for (const field of fields) {
        const reply = (await WSoftApi.djb({ op: 'read', field })).data;
        if (!reply.ok || reply.value === undefined) throw new Error(plainError(reply.error) ?? 'The weighing card did not answer.');
        const value = reply.value;
        setCardValues((old) => ({ ...old, [field]: value }));
      }
      return true;
    } catch (error) {
      if (!quiet) enqueueSnackbar(boardErrorText(error, 'The weighing card did not answer.'), { variant: 'error' });
      return false;
    } finally {
      setCardBusy(false);
    }
  }, [enqueueSnackbar]);

  // The filters live only on the card: read them once so the form starts from what it holds.
  useEffect(() => {
    if (setup?.source === 'rs485' && isAdmin && !filtersAsked.current) {
      filtersAsked.current = true;
      readCard(['filter1', 'filter2'], true);
    }
  }, [setup?.source, isAdmin, readCard]);

  const content = () => {
    if (!setup || !form) {
      return (<FormLoader onRetry={load} errorMessage={loadError} />);
    }

    if (setup.source === 'rs232') {
      return (
        <Typography variant="body1">
          Set on the indicator (weight, tare, zero, calibration, SA number and setup live there).
        </Typography>
      );
    }

    // A load cell on an HX711 board: the board is the scale, so everything is kept here. The
    // unit and the division take the place of the card's two fields; the rest is the same form.
    const loadcell = setup.source === 'loadcell';
    const divisions = loadcellDivisions(cellForm.unit);
    const division = divisions.find((d) => d.key === cellForm.division);
    const cellChanged = (next: Partial<typeof cellForm>) => {
      const merged = { ...cellForm, ...next };
      // kg and g have different lists: keep the choice when it is in both, else start at 1.
      if (next.unit && !loadcellDivisions(next.unit).some((d) => d.key === merged.division)) merged.division = divisionKey(0, 1);
      setCellForm(merged);
      setDirty(true);
    };
    const cellDiffers = loadcell && !!division && (!setup.card || setup.card.decimal !== division.decimals
      || setup.card.division !== division.step || loadcellUnit(setup.card.unit) !== cellForm.unit
      || (!!cell && (cell.filter !== cellForm.filter || cell.zero_track !== cellForm.zeroTrack
        || cell.drift_guard !== cellForm.driftGuard)));

    const canEdit = isAdmin && setup.editable && setup.locked === null;
    const change = (key: keyof BoardForm, value: string | boolean) => {
      setForm({ ...form, [key]: value });
      setDirty(true);
    };

    const problems: Partial<Record<keyof BoardForm, string>> = {
      sa_number: form.sa_number.trim().length > 32 ? 'Up to 32 characters' : undefined,
      max_kg: kgProblem(form.max_kg),
      min_kg: kgProblem(form.min_kg),
      blank_below_e: wholeProblem(form.blank_below_e, false),
      blank_above_e: wholeProblem(form.blank_above_e, true),
      stable_samples: wholeProblem(form.stable_samples, true),
      tare_max_kg: kgProblem(form.tare_max_kg),
      preset_tare_kg: kgProblem(form.preset_tare_kg)
    };
    const rsChanged = (key: keyof Rs485Form, value: string) => {
      setRsForm({ ...rsForm, [key]: value });
      setDirty(true);
    };
    const rsProblems = loadcell ? {} : rs485Problems(rsForm);
    const step = eStep(rsForm.decimal, rsForm.e_kg);

    const reasonText = reasonProblem(reason);
    const formOk = Object.values(problems).every((problem) => !problem) && Object.keys(rsProblems).length === 0;

    // A DJB-W1X has no unit, decimal places or e: the board keeps them (sent with the setup below).
    const onBoard = !!setup.format_on_board;
    const typedFormat = step !== undefined ? { decimal: Number(rsForm.decimal), division: step, unit: Number(rsForm.unit) } : undefined;
    const format = onBoard && typedFormat && (!setup.card || setup.card.decimal !== typedFormat.decimal
      || setup.card.division !== typedFormat.division || setup.card.unit !== typedFormat.unit) ? typedFormat : undefined;
    // Stable and auto zero: the board always judges stable itself, so it always keeps its copy.
    const tuning = !loadcell && setup.tuning ? formTuning(rsForm) : undefined;
    const tuningChanged = !!tuning && !!setup.tuning && (Object.keys(tuning) as (keyof typeof tuning)[])
      .some((key) => tuning[key] !== setup.tuning?.[key]);

    /**
     * Writes every value the card keeps (unit and e first, so the board checks Max and Min against
     * the new e), reading each one back. Returns the labels the card did not keep.
     */
    const writeCard = async () => {
      const missed: string[] = [];
      for (const write of cardWrites(rsForm, onBoard, cardValues)) {
        const label = CHECK_ROWS.find((row) => row.field === write.field)?.label ?? write.field;
        const reply = (await WSoftApi.djb({ op: 'write', field: write.field, value: write.value })).data;
        if (!reply.ok) throw new Error(`${label}: ${plainError(reply.error) ?? 'the weighing card did not answer.'}`);
        const back = (await WSoftApi.djb({ op: 'read', field: write.field })).data;
        const value = back.ok ? back.value : undefined;
        setCardValues((old) => ({ ...old, [write.field]: value }));
        if (value !== write.value) missed.push(label);
      }
      return missed;
    };

    const writeCell = async () => {
      if (!cellDiffers || !division) return;
      setCell((await WSoftApi.updateWSoftLoadcell({
        action: 'format', decimals: division.decimals, step: division.step, unit: cellForm.unit, filter: cellForm.filter,
        zero_track: cellForm.zeroTrack, drift_guard: cellForm.driftGuard
      })).data);
    };

    const save = async () => {
      if (!formOk || reasonText) return;
      setSaving(true);
      try {
        const missed = loadcell ? [] : await writeCard();
        if (loadcell) await writeCell();
        const reply = (await WSoftApi.updateWSoftScaleSetup({
          board: toBoard(form), reason: reason.trim(), format, tuning: tuningChanged ? tuning : undefined
        })).data;
        setSetup(reply);
        setDirty(false);
        setReason('');
        if (missed.length > 0) {
          enqueueSnackbar(`Saved, but the card did not keep: ${missed.join(', ')}. See the check table.`, { variant: 'warning' });
        } else {
          enqueueSnackbar('Scale setup saved', { variant: 'success' });
        }
      } catch (error) {
        enqueueSnackbar(boardErrorText(error, 'Could not save the scale setup'), { variant: 'error' });
      } finally {
        setSaving(false);
      }
    };

    const rsField = (key: keyof Rs485Form, label: string, helper: string, choices?: { value: string; text: string }[]) => (
      <Grid item xs={12} sm={6}>
        <TextField
          select={!!choices}
          label={label}
          fullWidth
          margin="dense"
          value={rsForm[key]}
          disabled={!canEdit}
          onChange={(event) => rsChanged(key, event.target.value)}
          inputProps={choices ? undefined : { inputMode: 'decimal' }}
          error={!!rsProblems[key]}
          helperText={rsProblems[key] ?? helper}
        >
          {choices?.map((choice) => (<MenuItem key={choice.value} value={choice.value}>{choice.text}</MenuItem>))}
        </TextField>
      </Grid>
    );
    const where = (onCardToo: boolean) => (onCardToo
      ? 'Saved on the weighing card'
      : 'Kept on this board (the DJB-W1X has no such setting)');
    const filterChoices = (max: number) => Array.from({ length: max + 1 }, (_, value) => ({ value: String(value), text: String(value) }));

    const field = (key: Exclude<keyof BoardForm, 'tare_enabled'>, label: string, helper: string, numeric = true) => (
      <Grid item xs={12} sm={6}>
        <TextField
          label={label}
          fullWidth
          margin="dense"
          value={form[key]}
          disabled={!canEdit}
          onChange={(event) => change(key, event.target.value)}
          inputProps={numeric ? { inputMode: 'decimal' } : { maxLength: 32 }}
          error={!!problems[key]}
          helperText={problems[key] ?? helper}
        />
      </Grid>
    );

    return (
      <>
        <Box display="flex" gap={1} flexWrap="wrap" alignItems="center" my={2}>
          {setup.complete
            ? <Chip label="Setup complete" color="success" size="small" />
            : <Chip label="Setup not complete" color="warning" size="small" />}
          {setup.missing.map((key) => (
            <Chip key={key} label={`Missing: ${MISSING_LABEL[key] ?? key}`} variant="outlined" color="warning" size="small" />
          ))}
          <Typography variant="body2" color="text.secondary">Setup changes: {setup.setup_audit}</Typography>
        </Box>
        {setup.locked === 'APPROVAL_ON' && (
          <MessageBox level="warning" message="Switch Approval off to change the setup." my={2} />
        )}
        {!isAdmin && setup.locked === null && (
          <MessageBox level="info" message="Only an admin can change the setup." my={2} />
        )}
        <Typography variant="h6">On this board</Typography>
        <Grid container columnSpacing={2}>
          {field('sa_number', 'SA approval number', 'Shown on the screen plate. 1 to 32 characters', false)}
          {loadcell ? (
            <>
              <Grid item xs={12} sm={6}>
                <TextField
                  select
                  label="Unit"
                  fullWidth
                  margin="dense"
                  value={cellForm.unit}
                  disabled={!canEdit}
                  onChange={(event) => cellChanged({ unit: event.target.value as LoadcellUnit })}
                  helperText="What the scale shows its weight in"
                >
                  <MenuItem value="kg">kg</MenuItem>
                  <MenuItem value="g">g</MenuItem>
                </TextField>
              </Grid>
              <Grid item xs={12} sm={6}>
                <TextField
                  select
                  label="Division size (e)"
                  fullWidth
                  margin="dense"
                  value={division ? division.key : ''}
                  disabled={!canEdit}
                  onChange={(event) => cellChanged({ division: event.target.value })}
                  helperText="The steps the weight moves in, like 0.002 kg"
                >
                  {divisions.map((d) => (<MenuItem key={d.key} value={d.key}>{d.text} {cellForm.unit}</MenuItem>))}
                </TextField>
              </Grid>
              <Grid item xs={12} sm={6}>
                <TextField
                  select
                  label="Filter"
                  fullWidth
                  margin="dense"
                  value={cellForm.filter}
                  disabled={!canEdit}
                  onChange={(event) => cellChanged({ filter: Number(event.target.value) })}
                  helperText={noiseHint(cell?.noise_tenths)}
                >
                  {LOADCELL_FILTERS.map((f) => (<MenuItem key={f.value} value={f.value}>{f.text}: {f.detail}</MenuItem>))}
                </TextField>
              </Grid>
              <Grid item xs={12} sm={6}>
                <TextField
                  select
                  label="Auto zero"
                  fullWidth
                  margin="dense"
                  value={cellForm.zeroTrack}
                  disabled={!canEdit}
                  onChange={(event) => cellChanged({ zeroTrack: Number(event.target.value) })}
                  helperText="Pulls the empty scale back to 0 when it creeps a little"
                >
                  {LOADCELL_AUTO_ZERO.map((z) => (<MenuItem key={z.value} value={z.value}>{z.text}</MenuItem>))}
                </TextField>
              </Grid>
              <Grid item xs={12} sm={6}>
                <TextField
                  select
                  label="Drift Guard"
                  fullWidth
                  margin="dense"
                  value={cellForm.driftGuard}
                  disabled={!canEdit}
                  onChange={(event) => cellChanged({ driftGuard: Number(event.target.value) })}
                  helperText="Holds a steady weight still when it wanders slowly. A slow change smaller than this stays hidden"
                >
                  {LOADCELL_DRIFT_GUARD.map((d) => (<MenuItem key={d.value} value={d.value}>{d.text}</MenuItem>))}
                </TextField>
              </Grid>
            </>
          ) : (
            <>
              {rsField('unit', 'Unit', where(!onBoard), UNITS.map((unit) => ({ value: String(unit.value), text: unit.text })))}
              {rsField('decimal', 'Decimal places', `Digits after the point, 0 to 4. ${where(!onBoard)}`,
                [0, 1, 2, 3, 4].map((value) => ({ value: String(value), text: String(value) })))}
              {rsField('e_kg', 'e (division size)', `The step the weight moves in, like 1, 0.5 or 0.02. ${where(!onBoard)}`)}
              {rsField('stable_range', 'Stable range (digits)',
                `How far the reading may wobble and still count as still. 0 = must not move. ${where(!onBoard)}`)}
              {rsField('stable_time', 'Stable time (s)',
                `How long it must hold still, 0.1 to 9.9 s. 0 = off. ${where(!onBoard)}`)}
              {rsField('zero_track_range', 'Auto zero range (digits)',
                `Pulls a creeping empty scale back to 0 within this many digits. 0 = off. ${where(!onBoard)}`)}
              {rsField('zero_track_time', 'Auto zero time (s)', `How long it must sit near 0 first, 0.1 to 9.9 s. ${where(!onBoard)}`)}
              {rsField('filter1', 'Filter 1', `0 to ${FILTER1_MAX}, higher = calmer but slower. Saved on the weighing card`,
                filterChoices(FILTER1_MAX))}
              {rsField('filter2', 'Filter 2', `0 to ${FILTER2_MAX}, higher = calmer but slower. Saved on the weighing card`,
                filterChoices(FILTER2_MAX))}
            </>
          )}
          {field('max_kg', 'Max (kg)', 'More than e')}
          {field('min_kg', 'Min (kg)', 'At least e and less than Max')}
          {field('stable_samples', 'Stable samples', 'Readings in a row before the weight counts as stable. 3 to 20')}
          {field('blank_below_e', 'Blank below zero (e)', 'Show ---- this many steps below zero. 0 to 100')}
          {field('blank_above_e', 'Blank above Max (e)', 'Show ---- this many steps above Max. 0 to 100')}
          <Grid item xs={12}>
            <FormControlLabel
              control={
                <Checkbox
                  checked={form.tare_enabled}
                  disabled={!canEdit}
                  onChange={(event) => change('tare_enabled', event.target.checked)}
                />
              }
              label="Tare allowed (the TARE button on the screen)"
            />
          </Grid>
          {field('tare_max_kg', 'Most tare (kg)', 'Up to Max. Empty = not set')}
          {field('preset_tare_kg', 'Preset tare (kg)', 'Up to the most tare. Empty = none')}
        </Grid>
        {canEdit && (
          <>
            <TextField
              label="Reason for this change"
              fullWidth
              margin="normal"
              value={reason}
              onChange={(event) => setReason(event.target.value)}
              inputProps={{ maxLength: REASON_MAX }}
              error={reason.length > 0 && !!reasonText}
              helperText={(reason.length > 0 && reasonText) || `Logged with your name. ${reason.trim().length} of ${REASON_MAX}`}
            />
            <ButtonRow mt={1}>
              <Button
                startIcon={<SaveIcon />}
                disabled={saving || !formOk || !!reasonText}
                variant="contained"
                color="primary"
                onClick={save}
              >
                Save
              </Button>
              {dirty && (
                <Button variant="outlined" color="secondary" disabled={saving} onClick={() => setDirty(false)}>
                  Undo changes
                </Button>
              )}
            </ButtonRow>
          </>
        )}
        {!loadcell && (
          <WSoftRs485Check
            setup={setup}
            form={rsForm}
            card={cardValues}
            board={boardValues(setup.card, setup.tuning)}
            canRead={isAdmin}
            busy={cardBusy || saving}
            onRead={readCard}
          />
        )}
      </>
    );
  };

  return (
    <SectionContent title="Scale setup" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default WSoftScaleSetup;
