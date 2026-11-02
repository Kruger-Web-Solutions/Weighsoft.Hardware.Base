import { FC, useCallback, useEffect, useState } from 'react';
import { AxiosError } from 'axios';

import {
  Alert, Box, Button, Chip, Dialog, DialogActions, DialogContent, DialogTitle, TextField, Typography
} from '@mui/material';
import LinkIcon from '@mui/icons-material/Link';
import LinkOffIcon from '@mui/icons-material/LinkOff';

import { ButtonRow, FormLoader, SectionContent } from '../../components';
import { extractErrorMessage } from '../../utils';

import * as WSoftApi from './api';
import { WSoftHubStatus } from './types';

const POLL_MS = 3000;

// Plain words for each state the box can be in (same words as its screen).
const STATE_HELP: Record<string, string> = {
  'HUB ONLINE': 'Linked. The Hub is receiving heartbeats from this box.',
  'HUB OFFLINE': 'The Hub can not be reached. Is the Hub running, and is its address right?',
  'HUB REJECTED': 'The Hub refused this box. The code may be revoked or used up: make a new code on the Hub.',
  'HUB CLOCK': 'Waiting for the time. The box asks the Hub for it every few seconds.',
  'HUB NO WIFI': 'This box is not on Wi-Fi.',
  'HUB SETUP': 'Not linked yet. Paste a pairing code below.'
};

// The Hub's other LAN addresses from the pairing code, without the one in use.
const otherAddresses = (status: WSoftHubStatus): string[] =>
  (status.hub_urls ?? []).filter((url) => url !== status.hub_url);

const WSoftHub: FC = () => {
  const [status, setStatus] = useState<WSoftHubStatus>();
  const [loadError, setLoadError] = useState<string>();
  const [code, setCode] = useState('');
  const [busy, setBusy] = useState(false);
  const [codeError, setCodeError] = useState<string>();
  const [restarting, setRestarting] = useState<string>();
  const [confirmUnlink, setConfirmUnlink] = useState(false);

  const load = useCallback(async () => {
    try {
      const response = await WSoftApi.readWSoftHub();
      setStatus(response.data);
      setLoadError(undefined);
      setRestarting(undefined);
    } catch (error: any) {
      // While the box restarts the page can't reach it; keep the "restarting" note up.
      setLoadError(extractErrorMessage(error as AxiosError<{ message?: string }>, 'Problem loading the Hub link'));
    }
  }, []);

  useEffect(() => {
    load();
    const timer = setInterval(load, POLL_MS);
    return () => clearInterval(timer);
  }, [load]);

  const link = async () => {
    setBusy(true);
    setCodeError(undefined);
    try {
      const response = await WSoftApi.pairWSoftHub(code.trim());
      setCode('');
      setRestarting(
        'Code saved. The box is restarting to link to ' + (response.data.hub_url ?? 'the Hub') +
        '. This page updates by itself in about 20 seconds.'
      );
    } catch (error: any) {
      setCodeError(extractErrorMessage(error as AxiosError<{ message?: string }>, 'The box did not accept the code'));
    } finally {
      setBusy(false);
    }
  };

  const unlink = async () => {
    setConfirmUnlink(false);
    setBusy(true);
    try {
      await WSoftApi.unpairWSoftHub();
      setRestarting('Hub link removed. The box is restarting.');
    } catch (error: any) {
      setCodeError(extractErrorMessage(error as AxiosError<{ message?: string }>, 'Could not remove the Hub link'));
    } finally {
      setBusy(false);
    }
  };

  if (!status) {
    return (
      <SectionContent title="Hub" titleGutter>
        <FormLoader onRetry={load} errorMessage={loadError} />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Hub" titleGutter>
      {restarting && <Alert severity="info" sx={{ mb: 2 }}>{restarting}</Alert>}

      <Box display="flex" alignItems="center" gap={1} mb={1}>
        <Typography variant="subtitle1">Status:</Typography>
        <Chip label={status.state} color={status.online ? 'success' : status.configured ? 'warning' : 'default'} />
      </Box>
      <Typography variant="body2" color="text.secondary" paragraph>
        {STATE_HELP[status.state] ?? status.state}
      </Typography>

      {status.configured && (
        <Box mb={2}>
          <Typography variant="body2">Sending to: <b>{status.hub_url}</b></Typography>
          {otherAddresses(status).length > 0 && (
            <Typography variant="body2" color="text.secondary">
              Other Hub addresses, tried in turn if this one stops answering: {otherAddresses(status).join(', ')}
            </Typography>
          )}
          <Typography variant="body2">Key: {status.key_id}</Typography>
          <Typography variant="body2">
            Last Hub answer: {status.last_status ? status.last_status : 'none yet'}
            {status.clock_from_hub ? ' · clock set from the Hub' : ''}
          </Typography>
        </Box>
      )}

      {status.warning && <Alert severity="warning" sx={{ mb: 2 }}>{status.warning}</Alert>}

      <Typography variant="h6" gutterBottom>
        {status.configured ? 'Link to a different Hub' : 'Link to the Hub'}
      </Typography>
      <Typography variant="body2" paragraph>
        On the Hub: Devices, then New pairing code, then Copy code. Paste it here.
        A code from an up-to-date Hub lists every Hub address; this box uses the local network ones.
      </Typography>
      <TextField
        label="Pairing code"
        fullWidth
        multiline
        minRows={3}
        variant="outlined"
        value={code}
        onChange={(event) => { setCode(event.target.value); setCodeError(undefined); }}
        error={!!codeError}
        helperText={codeError}
        inputProps={{ spellCheck: false, autoComplete: 'off', 'aria-label': 'Pairing code' }}
      />
      <ButtonRow mt={1}>
        <Button
          startIcon={<LinkIcon />}
          variant="contained"
          color="primary"
          disabled={busy || !code.trim()}
          onClick={link}
        >
          Link to Hub
        </Button>
        {status.configured && (
          <Button
            startIcon={<LinkOffIcon />}
            variant="outlined"
            color="secondary"
            disabled={busy}
            onClick={() => setConfirmUnlink(true)}
          >
            Unlink
          </Button>
        )}
      </ButtonRow>

      <Dialog open={confirmUnlink} onClose={() => setConfirmUnlink(false)}>
        <DialogTitle>Unlink from the Hub?</DialogTitle>
        <DialogContent>
          The box stops sending heartbeats and restarts. Weighing is not affected.
          To link again you need a new code from the Hub.
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setConfirmUnlink(false)}>Cancel</Button>
          <Button color="secondary" variant="contained" onClick={unlink}>Unlink</Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default WSoftHub;
