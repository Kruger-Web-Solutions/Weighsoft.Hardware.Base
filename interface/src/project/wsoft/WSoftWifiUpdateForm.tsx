import { FC, useCallback, useEffect, useState } from 'react';
import { useSnackbar } from 'notistack';

import { Box, Button, Chip, Switch, TextField, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { BlockFormControlLabel, ButtonRow, MessageBox, SectionContent } from '../../components';

import * as WSoftApi from './api';
import { boardErrorText } from './errors';
import { WSoftWifiUpdate, WSoftWifiUpdateRequest } from './types';
import { passwordProblem, wifiUpdateChip } from './wifiUpdate';

// Admin only (the Settings tab). New software over Wi-Fi, so the board does not have to be
// plugged into a laptop. OFF by default; the board's own password; locked while Approval is ON.
// The board never sends the password back, so this page never shows it.
const WSoftWifiUpdateForm: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const [info, setInfo] = useState<WSoftWifiUpdate>();
  const [password, setPassword] = useState('');
  const [again, setAgain] = useState('');
  const [busy, setBusy] = useState(false);

  const load = useCallback(async () => {
    try {
      setInfo((await WSoftApi.readWSoftWifiUpdate()).data);
    } catch {
      setInfo(undefined);
    }
  }, []);

  useEffect(() => {
    load();
  }, [load]);

  if (!info) {
    return (
      <SectionContent title="Wi-Fi update" titleGutter>
        <Typography variant="body2" color="text.secondary">Could not read the Wi-Fi update switch.</Typography>
      </SectionContent>
    );
  }

  const send = async (request: WSoftWifiUpdateRequest, done: string) => {
    setBusy(true);
    try {
      setInfo((await WSoftApi.updateWSoftWifiUpdate(request)).data);
      enqueueSnackbar(done, { variant: 'success' });
    } catch (error) {
      enqueueSnackbar(boardErrorText(error, 'Could not change Wi-Fi update'), { variant: 'error' });
      load();
    } finally {
      setPassword('');
      setAgain('');
      setBusy(false);
    }
  };

  const chip = wifiUpdateChip(info);

  if (!info.possible) {
    return (
      <SectionContent title="Wi-Fi update" titleGutter>
        <Box display="flex" alignItems="center" gap={1} mb={1}>
          <Chip label={chip.label} color={chip.color} size="small" />
        </Box>
        <MessageBox
          level="info"
          message={
            'This board has room for one copy of the software only, so it cannot update over Wi-Fi. ' +
            'New software goes on with the USB cable.'
          }
          my={1}
        />
      </SectionContent>
    );
  }

  const locked = info.approval_on;
  const formatProblem = passwordProblem(password, info.password_min, info.password_max);
  const matchProblem = again.length > 0 && again !== password ? 'The two passwords are not the same' : undefined;
  const passwordReady = password.length > 0 && !formatProblem && password === again;

  return (
    <SectionContent title="Wi-Fi update" titleGutter>
      <Box display="flex" alignItems="center" gap={1} mb={1}>
        <Chip label={chip.label} color={chip.color} size="small" />
        <Chip
          label={info.password_set ? 'Password set' : 'No password yet'}
          color={info.password_set ? 'success' : 'warning'}
          size="small"
        />
      </Box>
      <Typography variant="body2" color="text.secondary">
        New software can be sent to this board over Wi-Fi, without a cable. It is off until you switch it on, and it
        needs this board&apos;s own password. Every change and every update is counted on the audit counter.
      </Typography>
      {locked && (
        <MessageBox level="warning" message="Locked while Approval is ON. Switch Approval OFF first." my={1} />
      )}
      <BlockFormControlLabel
        control={
          <Switch
            checked={info.on}
            disabled={busy || (!info.on && (locked || !info.password_set))}
            onChange={(event) =>
              send({ on: event.target.checked }, event.target.checked ? 'Wi-Fi update is ON' : 'Wi-Fi update is OFF')
            }
          />
        }
        label="Allow updates over Wi-Fi?"
      />
      {!info.password_set && (
        <Typography variant="body2" color="text.secondary">
          Set a password first. Then the switch can go on.
        </Typography>
      )}
      <TextField
        label={info.password_set ? 'New update password' : 'Update password'}
        type="password"
        fullWidth
        margin="normal"
        autoComplete="new-password"
        disabled={locked}
        value={password}
        onChange={(event) => setPassword(event.target.value)}
        inputProps={{ maxLength: info.password_max }}
        error={!!formatProblem}
        helperText={formatProblem ?? `${info.password_min} to ${info.password_max} characters, no spaces. Only for this board.`}
      />
      <TextField
        label="Type the password again"
        type="password"
        fullWidth
        margin="normal"
        autoComplete="new-password"
        disabled={locked}
        value={again}
        onChange={(event) => setAgain(event.target.value)}
        inputProps={{ maxLength: info.password_max }}
        error={!!matchProblem}
        helperText={matchProblem}
      />
      <ButtonRow mt={1}>
        <Button
          startIcon={<SaveIcon />}
          disabled={busy || locked || !passwordReady}
          variant="contained"
          color="primary"
          onClick={() => send({ password }, 'Update password saved')}
        >
          {info.password_set ? 'Change password' : 'Set password'}
        </Button>
      </ButtonRow>
      <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
        Software on this board now: {info.version}. Update port: {info.port}.
      </Typography>
    </SectionContent>
  );
};

export default WSoftWifiUpdateForm;
