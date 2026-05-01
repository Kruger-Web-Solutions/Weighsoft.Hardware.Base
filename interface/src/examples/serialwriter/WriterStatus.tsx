import React, { FC, useEffect } from 'react';
import { Box, Card, CardContent, TextField, Typography, Alert, Chip, Button } from '@mui/material';
import { SectionContent, FormLoader } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriter, readWriterSource } from '../../api/serialWriter';
import { SerialWriterData, SerialWriterForwarderData } from '../../types/serialWriter';

const sourceLabels = ['none', 'manual', 'rest', 'ws', 'mqtt', 'reader'];

const WriterStatus: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({ read: readSerialWriter, update: updateSerialWriter });
  const [forwarder, setForwarder] = React.useState<SerialWriterForwarderData | null>(null);

  useEffect(() => {
    const tick = () => readWriterSource().then((r) => setForwarder(r.data)).catch(() => {});
    tick();
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, []);

  if (!data) return <SectionContent title="Status" titleGutter><FormLoader onRetry={loadData} errorMessage={errorMessage} /></SectionContent>;

  return (
    <SectionContent title="Status" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>This Writer's current state and link to its source Reader.</Alert>

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
          <Typography variant="h6" gutterBottom>Source Reader</Typography>
          {forwarder ? (
            forwarder.connected ? (
              <Alert severity="success">Connected to <code>{forwarder.source_url}</code> · last received <code>{forwarder.last_received || '(none yet)'}</code></Alert>
            ) : forwarder.enabled ? (
              <Alert severity="warning">Reconnecting (attempt {forwarder.reconnect_attempts})… {forwarder.last_error}</Alert>
            ) : (
              <Alert severity="info">No source Reader configured. Open Settings to set one.</Alert>
            )
          ) : (
            <Typography color="text.secondary">Loading…</Typography>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Typography variant="h6" gutterBottom>Local outbound</Typography>
          <Typography>Last sent: <code>{data.last_sent || '(nothing yet)'}</code></Typography>
          <Typography>Last source: <Chip size="small" label={sourceLabels[data.last_sent_source] || 'none'} /></Typography>
          <Typography>Total bytes sent: {data.bytes_sent_total}</Typography>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterStatus;
