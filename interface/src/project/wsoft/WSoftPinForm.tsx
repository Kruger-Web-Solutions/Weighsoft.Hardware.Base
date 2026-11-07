import { FC, useCallback, useEffect, useState } from 'react';
import { useSnackbar } from 'notistack';

import { Box, Button, Chip, TextField, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { ButtonRow, SectionContent } from '../../components';

import * as WSoftApi from './api';
import { boardErrorText } from './errors';

const PIN_RULE = /^[0-9]{4,6}$/;

// Admin only (the Settings tab). Sets the PIN the screen asks for before it switches Approval.
// The board stores only a salted hash; it never sends the PIN back, so this page never shows it.
const WSoftPinForm: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const [pinSet, setPinSet] = useState<boolean>();
  const [pin, setPin] = useState('');
  const [again, setAgain] = useState('');
  const [busy, setBusy] = useState(false);

  const load = useCallback(async () => {
    try {
      setPinSet((await WSoftApi.readWSoftSwitches()).data.pin_set);
    } catch {
      setPinSet(undefined);
    }
  }, []);

  useEffect(() => {
    load();
  }, [load]);

  const formatProblem = pin.length > 0 && !PIN_RULE.test(pin) ? '4 to 6 digits' : undefined;
  const matchProblem = again.length > 0 && again !== pin ? 'The two PINs are not the same' : undefined;
  const ready = PIN_RULE.test(pin) && pin === again;

  const save = async () => {
    if (!ready) return;
    setBusy(true);
    try {
      setPinSet((await WSoftApi.updateWSoftPin({ pin })).data.pin_set);
      enqueueSnackbar('Approval PIN saved', { variant: 'success' });
    } catch (error) {
      enqueueSnackbar(boardErrorText(error, 'Could not save the PIN'), { variant: 'error' });
    } finally {
      setPin('');
      setAgain('');
      setBusy(false);
    }
  };

  const digitsOnly = (value: string) => value.replace(/[^0-9]/g, '').slice(0, 6);

  return (
    <SectionContent title="Approval PIN" titleGutter>
      <Box display="flex" alignItems="center" gap={1} mb={1}>
        <Chip
          label={pinSet === undefined ? 'Unknown' : pinSet ? 'PIN set' : 'No PIN yet'}
          color={pinSet ? 'success' : 'warning'}
          size="small"
        />
      </Box>
      <Typography variant="body2" color="text.secondary">
        The screen asks for this PIN before it switches Approval. The board keeps only a scrambled copy and never
        shows the PIN again.
      </Typography>
      <TextField
        label={pinSet ? 'New PIN' : 'PIN'}
        type="password"
        fullWidth
        margin="normal"
        autoComplete="new-password"
        value={pin}
        onChange={(event) => setPin(digitsOnly(event.target.value))}
        inputProps={{ inputMode: 'numeric', pattern: '[0-9]*', maxLength: 6 }}
        error={!!formatProblem}
        helperText={formatProblem ?? '4 to 6 digits'}
      />
      <TextField
        label="Type the PIN again"
        type="password"
        fullWidth
        margin="normal"
        autoComplete="new-password"
        value={again}
        onChange={(event) => setAgain(digitsOnly(event.target.value))}
        inputProps={{ inputMode: 'numeric', pattern: '[0-9]*', maxLength: 6 }}
        error={!!matchProblem}
        helperText={matchProblem}
      />
      <ButtonRow mt={1}>
        <Button startIcon={<SaveIcon />} disabled={busy || !ready} variant="contained" color="primary" onClick={save}>
          {pinSet ? 'Change PIN' : 'Set PIN'}
        </Button>
      </ButtonRow>
    </SectionContent>
  );
};

export default WSoftPinForm;
