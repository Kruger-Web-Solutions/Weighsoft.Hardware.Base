import { FC, useCallback, useEffect, useState } from 'react';
import { AxiosError } from 'axios';
import { useSnackbar } from 'notistack';

import { Box, Button, Paper, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { extractErrorMessage } from '../../utils';

import * as WSoftApi from './api';
import { WSoftSave, WSoftWeighing } from './types';

const REFRESH_MS = 5000;

const describe = (w: WSoftWeighing) =>
  `#${w.seq}: ${w.weight} ${w.unit} at ${new Date(w.at * 1000).toLocaleTimeString()}`;

// Save button for the Live page: the same as the SAVE button on the board's screen.
const WSoftSavePanel: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const [save, setSave] = useState<WSoftSave>();
  const [busy, setBusy] = useState(false);

  const load = useCallback(async () => {
    try {
      setSave((await WSoftApi.readWSoftSave()).data);
    } catch {
      // The live weight above shows when the board is unreachable.
    }
  }, []);

  useEffect(() => {
    load();
    const timer = setInterval(load, REFRESH_MS);
    return () => clearInterval(timer);
  }, [load]);

  const onSave = async () => {
    setBusy(true);
    try {
      const reply = (await WSoftApi.saveWSoftWeighing()).data;
      setSave(reply);
      enqueueSnackbar(reply.saved ? `Saved ${describe(reply.saved)}` : 'Saved', { variant: 'success' });
    } catch (error: any) {
      const axiosError = error as AxiosError<WSoftSave>;
      const reason = axiosError.response?.data?.result;
      if (axiosError.response?.data) setSave(axiosError.response.data);
      enqueueSnackbar(
        reason ? `Not saved: ${reason}` : extractErrorMessage(error as AxiosError<{ message?: string }>, 'Not saved'),
        { variant: 'warning' }
      );
    } finally {
      setBusy(false);
    }
  };

  return (
    <Paper variant="outlined" sx={{ p: 2, mb: 2 }}>
      <Box display="flex" alignItems="center" gap={2} flexWrap="wrap">
        <Button startIcon={<SaveIcon />} variant="contained" color="primary" size="large" disabled={busy} onClick={onSave}>
          Save weighing
        </Button>
        <Box>
          <Typography variant="body2">
            {save?.auto_save ? 'Auto save is on: each settled load saves by itself.' : 'Saves the settled load for the Hub.'}
          </Typography>
          <Typography variant="body2" color="text.secondary">
            {save?.last ? `Last saved ${describe(save.last)}` : 'Nothing saved since the board started.'}
            {save && save.pending > 0 ? ` · ${save.pending} waiting for the Hub` : ''}
          </Typography>
        </Box>
      </Box>
    </Paper>
  );
};

export default WSoftSavePanel;
