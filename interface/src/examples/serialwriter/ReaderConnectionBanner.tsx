import React, { FC, useContext, useEffect, useState } from 'react';
import { Alert, Box, Button, Chip, Typography } from '@mui/material';
import SettingsIcon from '@mui/icons-material/Settings';
import RefreshIcon from '@mui/icons-material/Refresh';
import { Link as RouterLink } from 'react-router-dom';

import { readWriterSource } from '../../api/serialWriter';
import { FeaturesContext } from '../../contexts/features';
import { SerialWriterForwarderData } from '../../types/serialWriter';
import {
  connectionMethodLabel,
  readerConnectionState,
  readerHostFromSourceUrl,
  READER_UNREACHABLE_HINT,
} from './serialWriterSource';

interface ReaderConnectionBannerProps {
  pollMs?: number;
}

const ReaderConnectionBanner: FC<ReaderConnectionBannerProps> = ({ pollMs = 2000 }) => {
  const [source, setSource] = useState<SerialWriterForwarderData | null>(null);
  // A W-Soft Writer follows a weigh board; other Writers follow a Serial Reader.
  const { features } = useContext(FeaturesContext);
  const name = features.wsoft ? 'weigh board' : 'Serial Reader';
  const [loadingError, setLoadingError] = useState<string>('');

  const refresh = React.useCallback(() => {
    readWriterSource()
      .then((response) => {
        setSource(response.data);
        setLoadingError('');
      })
      .catch(() => setLoadingError('Cannot read this Writer source status right now.'));
  }, []);

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, pollMs);
    return () => clearInterval(id);
  }, [refresh, pollMs]);

  if (loadingError) {
    return (
      <Alert severity="error" sx={{ mb: 2 }} action={<Button color="inherit" size="small" onClick={refresh}>Retry</Button>}>
        {loadingError}
      </Alert>
    );
  }

  const state = readerConnectionState(source);
  const host = readerHostFromSourceUrl(source?.source_url || '');

  if (state === 'connected') {
    return (
      <Alert severity="success" sx={{ mb: 2 }}>
        <Box display="flex" flexWrap="wrap" gap={1} alignItems="center">
          <Typography component="span">
            Connected to the {name} <strong>{host}</strong>.
          </Typography>
          <Chip size="small" color="success" label={connectionMethodLabel(source!.connection_method)} />
          <Typography component="span" color="text.secondary">
            Last received: <code>{source!.last_received || 'waiting for scale line'}</code>
          </Typography>
        </Box>
      </Alert>
    );
  }

  if (state === 'not_configured') {
    return (
      <Alert
        severity="info"
        sx={{ mb: 2 }}
        action={<Button color="inherit" size="small" component={RouterLink} to="settings" startIcon={<SettingsIcon />}>Set up</Button>}
      >
        This Writer has no {name} yet. Open Settings, Step 1, and pick the {name} (the board with the scale).
        After that it reconnects by itself.
      </Alert>
    );
  }

  if (state === 'disabled') {
    return (
      <Alert
        severity="info"
        sx={{ mb: 2 }}
        action={<Button color="inherit" size="small" component={RouterLink} to="settings" startIcon={<SettingsIcon />}>Enable</Button>}
      >
        The {name} is saved but switched off. In Settings, Step 1, turn on Take data.
      </Alert>
    );
  }

  const reconnectText = source?.reconnect_attempts ? `Reconnect attempt ${source.reconnect_attempts}.` : 'Waiting for reconnect.';

  return (
    <Alert
      severity={state === 'error' ? 'error' : 'warning'}
      sx={{ mb: 2 }}
      action={<Button color="inherit" size="small" onClick={refresh} startIcon={<RefreshIcon />}>Refresh</Button>}
    >
      The {name} <strong>{host || 'unknown'}</strong> is not connected. {reconnectText}{' '}
      {source?.last_error ? <span>Last error: {source.last_error}</span> : <span>{READER_UNREACHABLE_HINT}</span>}
    </Alert>
  );
};

export default ReaderConnectionBanner;
