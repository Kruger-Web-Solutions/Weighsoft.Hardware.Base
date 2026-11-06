import { FC, useCallback, useEffect, useState } from 'react';
import { AxiosError } from 'axios';
import { useSnackbar } from 'notistack';

import { Box, Button, Chip, TextField, Typography } from '@mui/material';
import LinkIcon from '@mui/icons-material/Link';
import LinkOffIcon from '@mui/icons-material/LinkOff';

import { ButtonRow, FormLoader, MessageBox, SectionContent } from '../../components';
import { extractErrorMessage } from '../../utils';

import * as WSoftApi from './api';
import { WSoftHub } from './types';

const REFRESH_MS = 5000;

// Board-side errors come back as { error } with HTTP 400.
const errorText = (error: AxiosError<{ error?: string }>, fallback: string) =>
  error.response?.data?.error || extractErrorMessage(error as AxiosError<{ message?: string }>, fallback);

const stateColor = (hub: WSoftHub): 'success' | 'warning' | 'error' | 'default' => {
  if (!hub.paired) return 'default';
  if (hub.online) return 'success';
  if (hub.state === 'HUB REJECTED' || hub.state === 'HUB OFFLINE') return 'error';
  return 'warning';
};

const WSoftHubForm: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const [hub, setHub] = useState<WSoftHub>();
  const [loadError, setLoadError] = useState<string>();
  const [code, setCode] = useState('');
  const [busy, setBusy] = useState(false);
  const [restarting, setRestarting] = useState(false);

  const loadData = useCallback(async () => {
    try {
      setLoadError(undefined);
      setHub((await WSoftApi.readWSoftHub()).data);
    } catch (error: any) {
      setLoadError(errorText(error, 'Problem loading the Hub link'));
    }
  }, []);

  useEffect(() => {
    loadData();
    const timer = setInterval(loadData, REFRESH_MS);
    return () => clearInterval(timer);
  }, [loadData]);

  const send = async (action: 'connect' | 'disconnect') => {
    setBusy(true);
    try {
      const reply = action === 'connect'
        ? await WSoftApi.updateWSoftHub({ action, code })
        : await WSoftApi.updateWSoftHub({ action });
      setCode('');
      setHub(reply.data);
      setRestarting(true);
      enqueueSnackbar('Saved. The board is restarting.', { variant: 'success' });
      // Give the board time to restart before polling again.
      setTimeout(() => setRestarting(false), 15000);
    } catch (error: any) {
      enqueueSnackbar(errorText(error, 'Could not save'), { variant: 'error' });
    } finally {
      setBusy(false);
    }
  };

  const content = () => {
    if (!hub) {
      return (<FormLoader onRetry={loadData} errorMessage={loadError} />);
    }

    return (
      <>
        <Box display="flex" alignItems="center" gap={1} my={2}>
          <Typography variant="body1">Status:</Typography>
          <Chip label={hub.paired ? hub.state : 'NOT CONNECTED'} color={stateColor(hub)} />
        </Box>
        {restarting && (
          <MessageBox level="info" message="The board is restarting. This page updates by itself in about 15 seconds." my={2} />
        )}
        {hub.paired && (
          <Box mb={2}>
            <Typography variant="body2">Hub address: <b>{hub.hub_url}</b></Typography>
            {hub.device_id && <Typography variant="body2">Device: {hub.device_id}</Typography>}
            <Typography variant="body2" color="text.secondary">
              {hub.last_status
                ? `Last check-in: ${hub.last_age_s} s ago (answer ${hub.last_status})`
                : 'No check-in yet since the last restart.'}
            </Typography>
          </Box>
        )}
        {hub.paired && hub.state === 'HUB OFFLINE' && (
          <MessageBox
            level="warning"
            message={
              'The board cannot reach this Hub address. Check the board is on the same network as the Hub, ' +
              'or make a new key on a Hub address the board can reach.'
            }
            my={2}
          />
        )}
        {hub.paired && hub.state === 'HUB REJECTED' && (
          <MessageBox
            level="error"
            message="The Hub refused this board. The key may be old or removed. Make a new key on the Hub and connect again."
            my={2}
          />
        )}
        <TextField
          name="code"
          label={hub.paired ? 'New Hub pairing key' : 'Hub pairing key'}
          fullWidth
          multiline
          minRows={3}
          variant="outlined"
          value={code}
          onChange={(event) => setCode(event.target.value)}
          margin="normal"
          autoComplete="off"
          inputProps={{ spellCheck: false }}
          helperText={
            'On the Hub: Devices, open this device, press New pairing code, then Copy code. ' +
            'Paste it here. The board keeps it secret and never shows it again.'
          }
        />
        <ButtonRow mt={1}>
          <Button
            startIcon={<LinkIcon />}
            disabled={busy || restarting || code.trim().length === 0}
            variant="contained"
            color="primary"
            onClick={() => send('connect')}
          >
            Connect
          </Button>
          {hub.paired && (
            <Button
              startIcon={<LinkOffIcon />}
              disabled={busy || restarting}
              variant="outlined"
              color="secondary"
              onClick={() => send('disconnect')}
            >
              Disconnect
            </Button>
          )}
        </ButtonRow>
      </>
    );
  };

  return (
    <SectionContent title="Connect to the Hub" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default WSoftHubForm;
