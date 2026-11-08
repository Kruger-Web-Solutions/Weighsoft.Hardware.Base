import { FC, useCallback, useContext, useEffect, useState } from 'react';
import { useSnackbar } from 'notistack';

import { Box, Button, Checkbox, Chip, FormControlLabel, Grid, MenuItem, TextField, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { ButtonRow, FormLoader, MessageBox, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import WSoftCardSetup from './WSoftCardSetup';
import * as WSoftApi from './api';
import { boardErrorText } from './errors';
import { WSoftScaleBoard, WSoftScaleCard, WSoftScaleSetup as WSoftScaleSetupState } from './types';
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

// Decimal places and e live on the weighing card (written through /rest/djb); the form shows them in kg.
// The card keeps e as a whole step in its smallest digit: e 0.02 kg with 2 decimals = step 2.
interface CardForm {
  decimal: string;
  e_kg: string;
}

const toCardForm = (card: WSoftScaleCard | null): CardForm => {
  if (!card) return { decimal: '', e_kg: '' };
  return { decimal: String(card.decimal), e_kg: (card.division / 10 ** card.decimal).toFixed(card.decimal) };
};

/** The card step for e at these decimal places, or undefined when e is not a whole number of that digit. */
const cardStep = (form: CardForm) => {
  const decimal = Number(form.decimal);
  const e = Number(kgText(form.e_kg));
  if (form.decimal === '' || !KG.test(kgText(form.e_kg)) || e <= 0) return undefined;
  const step = Math.round(e * 10 ** decimal);
  return Math.abs(step / 10 ** decimal - e) < 1e-9 ? step : undefined;
};
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
  card: 'Read the weighing card'
};

const WSoftScaleSetup: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const { me } = useContext(AuthenticationContext);
  const isAdmin = !!me?.admin;
  const [setup, setSetup] = useState<WSoftScaleSetupState>();
  const [loadError, setLoadError] = useState<string>();
  const [form, setForm] = useState<BoardForm>();
  const [cardForm, setCardForm] = useState<CardForm>({ decimal: '', e_kg: '' });
  const [dirty, setDirty] = useState(false);
  const [reason, setReason] = useState('');
  const [saving, setSaving] = useState(false);

  const load = useCallback(async () => {
    try {
      setSetup((await WSoftApi.readWSoftScaleSetup()).data);
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
      setCardForm(toCardForm(setup.card));
    }
  }, [setup, dirty]);

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
    const cardChanged = (key: keyof CardForm, value: string) => {
      setCardForm({ ...cardForm, [key]: value });
      setDirty(true);
    };
    const step = cardStep(cardForm);
    const cardTouched = cardForm.decimal !== '' || cardForm.e_kg.trim() !== '';
    const eProblem = cardTouched && step === undefined
      ? 'Pick the decimal places, then e in kg with no more decimals than that (like 0.02 with 2)'
      : undefined;
    const cardDiffers = step !== undefined
      && (!setup.card || setup.card.decimal !== Number(cardForm.decimal) || setup.card.division !== step);

    const reasonText = reasonProblem(reason);
    const formOk = Object.values(problems).every((problem) => !problem) && !eProblem;

    // Card first (decimal places, then e), so the board checks Max and Min against the new e.
    const writeCard = async () => {
      if (!cardDiffers || step === undefined) return;
      for (const [field, value] of [['decimal', Number(cardForm.decimal)], ['division', step]] as const) {
        const reply = (await WSoftApi.djb({ op: 'write', field, value })).data;
        if (!reply.ok) throw new Error(reply.error ?? 'The weighing card did not answer.');
      }
    };

    const save = async () => {
      if (!formOk || reasonText) return;
      setSaving(true);
      try {
        await writeCard();
        const reply = (await WSoftApi.updateWSoftScaleSetup({ board: toBoard(form), reason: reason.trim() })).data;
        setSetup(reply);
        setForm(toForm(reply.board));
        setCardForm(toCardForm(reply.card));
        setDirty(false);
        setReason('');
        enqueueSnackbar('Scale setup saved', { variant: 'success' });
      } catch (error) {
        enqueueSnackbar(boardErrorText(error, 'Could not save the scale setup'), { variant: 'error' });
      } finally {
        setSaving(false);
      }
    };

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
          <Grid item xs={12} sm={6}>
            <TextField
              select
              label="Decimal places"
              fullWidth
              margin="dense"
              value={cardForm.decimal}
              disabled={!canEdit}
              onChange={(event) => cardChanged('decimal', event.target.value)}
              helperText="How many digits after the point. Saved on the weighing card"
            >
              {[0, 1, 2, 3, 4].map((value) => (
                <MenuItem key={value} value={String(value)}>{value}</MenuItem>
              ))}
            </TextField>
          </Grid>
          <Grid item xs={12} sm={6}>
            <TextField
              label="e (kg)"
              fullWidth
              margin="dense"
              value={cardForm.e_kg}
              disabled={!canEdit}
              onChange={(event) => cardChanged('e_kg', event.target.value)}
              inputProps={{ inputMode: 'decimal' }}
              error={!!eProblem}
              helperText={eProblem ?? 'The scale step, like 1, 0.5 or 0.02. Saved on the weighing card'}
            />
          </Grid>
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
        <WSoftCardSetup card={setup.card} canRead={isAdmin} canWrite={canEdit} onWritten={load} />
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
