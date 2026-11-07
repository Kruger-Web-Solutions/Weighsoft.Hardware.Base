import { FC, useCallback, useContext, useEffect, useState } from 'react';
import { Link as RouterLink } from 'react-router-dom';
import { useSnackbar } from 'notistack';

import {
  Accordion, AccordionDetails, AccordionSummary, Box, Button, Dialog, DialogActions, DialogContent, DialogContentText,
  DialogTitle, FormControlLabel, Link, Paper, Switch, TextField, Typography
} from '@mui/material';
import ExpandMoreIcon from '@mui/icons-material/ExpandMore';

import { AuthenticationContext } from '../../contexts/authentication';

import * as WSoftApi from './api';
import { boardErrorText, plainError } from './errors';
import { WSoftSwitches as WSoftSwitchesState, WSoftSwitchName } from './types';
import { REASON_MAX, reasonProblem } from './validators';

const REFRESH_MS = 2000;

const onOff = (on: boolean) => (on ? 'ON' : 'OFF');

// Approval and Auto save as the board holds them. The screen, the Hub and this card all switch the same state.
const WSoftSwitches: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const { me } = useContext(AuthenticationContext);
  const isAdmin = !!me?.admin;
  const [state, setState] = useState<WSoftSwitchesState>();
  const [loadError, setLoadError] = useState<string>();
  const [busy, setBusy] = useState(false);
  const [askApproval, setAskApproval] = useState<boolean>();
  const [reason, setReason] = useState('');

  const load = useCallback(async () => {
    try {
      setState((await WSoftApi.readWSoftSwitches()).data);
      setLoadError(undefined);
    } catch (error) {
      setLoadError(boardErrorText(error, 'Could not read the switches'));
    }
  }, []);

  useEffect(() => {
    load();
    const timer = setInterval(load, REFRESH_MS);
    return () => clearInterval(timer);
  }, [load]);

  const send = async (which: WSoftSwitchName, on: boolean, why: string) => {
    setBusy(true);
    try {
      setState((await WSoftApi.updateWSoftSwitch({ switch: which, on, reason: why.trim() })).data);
      const name = which === 'approval' ? 'Approval' : 'Auto save';
      enqueueSnackbar(`${name} is ${onOff(on)}`, { variant: 'success' });
      return true;
    } catch (error) {
      enqueueSnackbar(boardErrorText(error, 'Could not switch'), { variant: 'error' });
      return false;
    } finally {
      setBusy(false);
    }
  };

  const openApproval = (on: boolean) => {
    setReason('');
    setAskApproval(on);
  };

  const confirmApproval = async () => {
    if (askApproval === undefined || reasonProblem(reason)) return;
    if (await send('approval', askApproval, reason)) setAskApproval(undefined);
    load();
  };

  if (!state) {
    return (
      <Paper variant="outlined" sx={{ p: 2, mb: 2 }}>
        <Typography variant="body2" color="text.secondary">{loadError ?? 'Reading the switches…'}</Typography>
      </Paper>
    );
  }

  const { approval } = state;
  const blockedText = !approval.on && approval.blocked ? plainError(approval.blocked) : undefined;
  const approvalDisabled = !isAdmin || busy || (!approval.on && !approval.can_switch_on);
  const problem = reasonProblem(reason);

  return (
    <Paper variant="outlined" sx={{ p: 2, mb: 2 }}>
      <Typography variant="h6" gutterBottom>Switches</Typography>
      <Box display="flex" gap={4} flexWrap="wrap">
        <Box>
          <FormControlLabel
            control={
              <Switch
                checked={approval.on}
                disabled={approvalDisabled}
                onChange={(event) => openApproval(event.target.checked)}
              />
            }
            label={<strong>Approval {onOff(approval.on)}</strong>}
          />
          {blockedText && (
            <Typography variant="body2" color="warning.main">
              {blockedText}{' '}
              {approval.blocked === 'SETUP_INCOMPLETE' && (
                <Link component={RouterLink} to="../setup">Open Scale setup</Link>
              )}
            </Typography>
          )}
        </Box>
        <Box>
          <FormControlLabel
            control={
              <Switch
                checked={state.auto_save}
                disabled={!isAdmin || busy}
                onChange={(event) => send('auto_save', event.target.checked, '')}
              />
            }
            label={<strong>Auto save {onOff(state.auto_save)}</strong>}
          />
          <Typography variant="body2" color="text.secondary">
            {state.auto_save ? 'Each settled load saves by itself.' : 'Loads save only with the Save button.'}
          </Typography>
        </Box>
      </Box>
      <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
        {isAdmin
          ? 'The screen and the Hub switch the same state. Each Approval change is logged with your name.'
          : 'Only an admin can switch these here.'}
        {state.pin_locked_s > 0 ? ` The screen PIN pad is locked for ${state.pin_locked_s} s after wrong PINs.` : ''}
        {loadError ? ` (${loadError})` : ''}
      </Typography>
      <Accordion disableGutters variant="outlined" sx={{ mt: 2 }}>
        <AccordionSummary expandIcon={<ExpandMoreIcon />}>
          <Typography variant="body2">Approval log ({state.log.length})</Typography>
        </AccordionSummary>
        <AccordionDetails>
          {state.log.length === 0
            ? <Typography variant="body2" color="text.secondary">No changes logged yet.</Typography>
            : state.log.map((line, index) => (
              <Typography key={index} variant="body2" sx={{ fontFamily: 'monospace', wordBreak: 'break-word' }}>
                {line}
              </Typography>
            ))}
        </AccordionDetails>
      </Accordion>
      <Dialog open={askApproval !== undefined} onClose={() => setAskApproval(undefined)} fullWidth maxWidth="sm">
        <DialogTitle>Switch Approval {onOff(!!askApproval)}?</DialogTitle>
        <DialogContent dividers>
          <DialogContentText>
            The board logs your name and this reason, and the audit counter goes up by 1.
          </DialogContentText>
          <TextField
            autoFocus
            fullWidth
            margin="normal"
            label="Reason"
            value={reason}
            onChange={(event) => setReason(event.target.value)}
            inputProps={{ maxLength: REASON_MAX }}
            error={reason.length > 0 && !!problem}
            helperText={(reason.length > 0 && problem) || `${reason.trim().length} of ${REASON_MAX} characters`}
            onKeyDown={(event) => {
              if (event.key === 'Enter') confirmApproval();
            }}
          />
        </DialogContent>
        <DialogActions>
          <Button variant="contained" color="secondary" onClick={() => setAskApproval(undefined)}>Cancel</Button>
          <Button variant="contained" color="primary" disabled={busy || !!problem} onClick={confirmApproval}>
            Switch {onOff(!!askApproval)}
          </Button>
        </DialogActions>
      </Dialog>
    </Paper>
  );
};

export default WSoftSwitches;
