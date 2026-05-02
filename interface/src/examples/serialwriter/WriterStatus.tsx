import React, { FC, useEffect } from 'react';
import { Box, Button, Card, CardContent, Chip, Divider, TextField, Typography, Alert } from '@mui/material';
import SettingsIcon from '@mui/icons-material/Settings';
import { Link as RouterLink } from 'react-router-dom';
import { SectionContent, FormLoader } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriter, readWriterSource } from '../../api/serialWriter';
import { SerialWriterData, SerialWriterForwarderData } from '../../types/serialWriter';
import {
  connectionMethodLabel,
  readerConnectionState,
  readerHostFromSourceUrl,
  serialSummary,
} from './serialWriterSource';

const sourceLabels = ['none', 'manual', 'rest', 'ws', 'mqtt', 'reader'];

const formatObservedAgo = (observedAt: number | null): string => {
  if (!observedAt) {
    return 'not seen in this browser session';
  }
  const seconds = Math.floor((Date.now() - observedAt) / 1000);
  if (seconds < 5) {
    return 'just now';
  }
  if (seconds < 60) {
    return `${seconds}s ago`;
  }
  return `${Math.floor(seconds / 60)}m ago`;
};

const WriterStatus: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({
    read: readSerialWriter,
    update: updateSerialWriter,
  });
  const [forwarder, setForwarder] = React.useState<SerialWriterForwarderData | null>(null);
  const [lastReceivedAt, setLastReceivedAt] = React.useState<number | null>(null);

  useEffect(() => {
    const tick = () => readWriterSource().then((r) => {
      setForwarder((previous) => {
        if (r.data.last_received_at && r.data.last_received_at !== previous?.last_received_at) {
          setLastReceivedAt(Date.now());
        }
        return r.data;
      });
    }).catch(() => {});
    tick();
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, []);

  if (!data) {
    return (
      <SectionContent title="Status" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const connectionState = readerConnectionState(forwarder);
  const readerHost = readerHostFromSourceUrl(forwarder?.source_url || '');
  const connected = connectionState === 'connected';

  return (
    <SectionContent title="Status" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Use this page during setup. It shows whether this Writer is pulling scale lines from the Reader and whether anything
        left Serial1.
      </Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" gutterBottom>Friendly name</Typography>
          <TextField
            fullWidth
            value={data.friendly_name}
            onChange={(e) => setData((p) => p ? { ...p, friendly_name: e.target.value } : p)}
            placeholder={data.device_id}
            helperText="Shown on the source Reader's Writers tab."
          />
          <Box mt={1}><Button variant="contained" onClick={saveData} disabled={saving}>Save name</Button></Box>
        </CardContent>
      </Card>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Box display="flex" justifyContent="space-between" alignItems="flex-start" gap={2} flexWrap="wrap">
            <Box>
              <Typography variant="h6" gutterBottom>Serial Reader connection</Typography>
              <Typography color="text.secondary" variant="body2">
                The Reader is the device with the scale connected to it.
              </Typography>
            </Box>
            <Button size="small" component={RouterLink} to="../settings" startIcon={<SettingsIcon />}>
              Open setup
            </Button>
          </Box>
          <Divider sx={{ my: 2 }} />
          {forwarder ? (
            <Box>
              <Box display="flex" gap={1} flexWrap="wrap" mb={2}>
                <Chip
                  color={connected ? 'success' : forwarder.last_error ? 'error' : 'default'}
                  label={connected ? 'Connected' : 'Not connected'}
                />
                <Chip variant="outlined" label={readerHost || 'No Reader URL'} />
                <Chip variant="outlined" label={forwarder.enabled ? 'Enabled' : 'Disabled'} />
                <Chip variant="outlined" label={connectionMethodLabel(forwarder.connection_method)} />
              </Box>
              {connected ? (
                <Alert severity="success" sx={{ mb: 2 }}>
                  Reader link is live. New scale lines will be written out through this Writer.
                </Alert>
              ) : forwarder.enabled ? (
                <Alert severity={forwarder.last_error ? 'error' : 'warning'} sx={{ mb: 2 }}>
                  Trying to reconnect{forwarder.reconnect_attempts ? ` (attempt ${forwarder.reconnect_attempts})` : ''}.{' '}
                  {forwarder.last_error || 'Check Reader power, WiFi, and URL.'}
                </Alert>
              ) : (
                <Alert severity="info" sx={{ mb: 2 }}>Serial Reader Source is disabled or not set up yet.</Alert>
              )}
              <Typography variant="body2">Source URL: <code>{forwarder.source_url || '(not set)'}</code></Typography>
              <Typography variant="body2">
                Last received: <code>{forwarder.last_received || '(waiting for scale line)'}</code>
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Seen by this browser: {formatObservedAgo(lastReceivedAt)}
              </Typography>
            </Box>
          ) : (
            <Typography color="text.secondary">Loading…</Typography>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Typography variant="h6" gutterBottom>Writer output</Typography>
          <Typography variant="body2" color="text.secondary" paragraph>
            These values confirm what this Writer sent to its local serial output device.
          </Typography>
          <Typography>
            Serial port: <code>{serialSummary(data.baud_rate, data.data_bits, data.parity, data.stop_bits, data.line_ending)}</code>
          </Typography>
          <Typography>Last sent: <code>{data.last_sent || '(nothing yet)'}</code></Typography>
          <Typography>Last source: <Chip size="small" label={sourceLabels[data.last_sent_source] || 'none'} /></Typography>
          <Typography>Total bytes sent: {data.bytes_sent_total}</Typography>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterStatus;
