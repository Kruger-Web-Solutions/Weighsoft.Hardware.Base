import React, { FC, useEffect } from 'react';
import { useSnackbar } from 'notistack';
import {
  Box,
  Card,
  CardContent,
  TextField,
  MenuItem,
  FormControl,
  FormLabel,
  RadioGroup,
  FormControlLabel,
  Radio,
  Switch,
  Button,
  Alert,
  Typography,
  Chip,
  CircularProgress,
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import CableIcon from '@mui/icons-material/Cable';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest, extractErrorMessage } from '../../utils';
import { readSerialWriter, updateSerialWriter, readWriterSource, updateWriterSource } from '../../api/serialWriter';
import { SerialWriterData, SerialWriterForwarderData } from '../../types/serialWriter';
import {
  connectionMethodLabel,
  normalizeReaderSourceUrlForPersistence,
  readerConnectionState,
  readerHostFromSourceUrl,
} from './serialWriterSource';

const BAUDRATES = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400];

const WriterSettings: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({
    read: readSerialWriter,
    update: updateSerialWriter,
  });
  const [src, setSrc] = React.useState<SerialWriterForwarderData | null>(null);
  const [savingSrc, setSavingSrc] = React.useState(false);
  const [testingSrc, setTestingSrc] = React.useState(false);
  const [testMessage, setTestMessage] = React.useState<{
    severity: 'success' | 'warning' | 'error' | 'info';
    text: string;
  } | null>(null);

  const refreshSrc = React.useCallback(() => {
    readWriterSource().then((r) => setSrc(r.data)).catch(() => {});
  }, []);

  useEffect(() => { refreshSrc(); }, [refreshSrc]);

  if (!data) {
    return (
      <SectionContent title="Settings" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const setField = <K extends keyof SerialWriterData>(k: K) => (v: SerialWriterData[K]) =>
    setData((p) => p ? { ...p, [k]: v } : p);

  const setSrcField = <K extends keyof SerialWriterForwarderData>(k: K) => (v: SerialWriterForwarderData[K]) =>
    setSrc((p) => p ? { ...p, [k]: v } : p);

  const waitForReaderConnection = async (previousLastReceivedAt: number): Promise<SerialWriterForwarderData> => {
    let latest = src;
    for (let i = 0; i < 8; i++) {
      await new Promise((resolve) => setTimeout(resolve, 1000));
      latest = (await readWriterSource()).data;
      setSrc(latest);
      if (latest.connected || latest.last_received_at !== previousLastReceivedAt) {
        return latest;
      }
    }
    return latest || (await readWriterSource()).data;
  };

  const saveSrc = async (testAfterSave = false) => {
    if (!src) return;
    setSavingSrc(true);
    setTestMessage(null);
    const previousLastReceivedAt = src.last_received_at;
    try {
      // Send only the persisted fields; leave runtime fields out of the body
      // so the backend update() doesn't have to ignore stray keys.
      const normalizedSourceUrl = normalizeReaderSourceUrlForPersistence(
        src.source_url,
        src.connection_method as 0 | 1,
      );
      const response = await updateWriterSource({
        source_url: normalizedSourceUrl,
        connection_method: src.connection_method,
        auto_reconnect: src.auto_reconnect,
        enabled: testAfterSave ? true : src.enabled,
      });
      setSrc(response.data);
      enqueueSnackbar("Source Reader saved", { variant: 'success' });
      if (testAfterSave) {
        setTestingSrc(true);
        setTestMessage({ severity: 'info', text: 'Saved. Testing the Reader connection now...' });
        const tested = await waitForReaderConnection(previousLastReceivedAt);
        if (tested.connected) {
          setTestMessage({
            severity: 'success',
            text: `Connected to Reader ${readerHostFromSourceUrl(tested.source_url)}.`,
          });
        } else if (tested.last_received_at !== previousLastReceivedAt) {
          setTestMessage({ severity: 'success', text: `Reader data received: ${tested.last_received}` });
        } else {
          setTestMessage({
            severity: 'warning',
            text: tested.last_error || 'Saved, but this Writer is still waiting for the Reader. Check Reader power, WiFi, and URL.',
          });
        }
      } else {
        refreshSrc();
      }
    } catch (error: any) {
      const message = extractErrorMessage(error, "Failed to save Source Reader");
      enqueueSnackbar(message, { variant: 'error' });
      setTestMessage({ severity: 'error', text: message });
    } finally {
      setSavingSrc(false);
      setTestingSrc(false);
    }
  };

  const sourceState = readerConnectionState(src);
  const readerHost = readerHostFromSourceUrl(src?.source_url || '');

  return (
    <SectionContent title="Settings" titleGutter>
      <Alert severity="warning" sx={{ mb: 2 }}>Changing serial port settings restarts the connection.</Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" gutterBottom>Serial Reader Source</Typography>
          <Typography color="text.secondary" paragraph>
            Connect this Writer to the Reader that has the scale attached. Paste the Reader web address; the Writer will use
            the live serial stream.
          </Typography>
          {src && (
            <>
              <Box display="flex" flexWrap="wrap" gap={1} mb={2}>
                <Chip
                  color={sourceState === 'connected' ? 'success' : sourceState === 'error' ? 'error' : 'default'}
                  label={sourceState === 'connected' ? 'Connected' : sourceState === 'not_configured' ? 'Not set up' : sourceState}
                />
                {readerHost && <Chip variant="outlined" label={readerHost} />}
                <Chip variant="outlined" label={connectionMethodLabel(src.connection_method)} />
              </Box>
              <TextField
                fullWidth
                label="Reader web address"
                placeholder="http://192.168.2.60/"
                value={src.source_url}
                onChange={(e) => setSrcField('source_url')(e.target.value)}
                helperText={
                  'You can paste http://192.168.2.60/ or ws://192.168.2.60/ws/serial. It will be normalized when saved.'
                }
                sx={{ mb: 2 }}
              />
              <FormControl component="fieldset" sx={{ mb: 2 }}>
                <FormLabel>Source protocol</FormLabel>
                <RadioGroup
                  row
                  value={String(src.connection_method)}
                  onChange={(e) => setSrcField('connection_method')(Number(e.target.value) as 0 | 1)}
                >
                  <FormControlLabel value="0" control={<Radio />} label="WebSocket live stream" />
                  <FormControlLabel value="1" disabled control={<Radio />} label="HTTP polling (coming later)" />
                </RadioGroup>
              </FormControl>
              <FormControlLabel
                control={
                  <Switch checked={src.enabled} onChange={(e) => setSrcField('enabled')(e.target.checked)} />
                }
                label="Enable Serial Reader Source"
              />
              <FormControlLabel
                control={
                  <Switch checked={src.auto_reconnect} onChange={(e) => setSrcField('auto_reconnect')(e.target.checked)} />
                }
                label="Auto-reconnect"
              />
              {testMessage && <Alert severity={testMessage.severity} sx={{ mt: 2 }}>{testMessage.text}</Alert>}
              <ButtonRow mt={1}>
                <Button
                  startIcon={testingSrc ? <CircularProgress size={16} color="inherit" /> : <CableIcon />}
                  variant="contained"
                  disabled={savingSrc || testingSrc || !src.source_url.trim()}
                  onClick={() => saveSrc(true)}
                >
                  Save Reader and Test Connection
                </Button>
                <Button startIcon={<SaveIcon />} disabled={savingSrc || testingSrc} onClick={() => saveSrc(false)}>Save only</Button>
              </ButtonRow>
            </>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Typography variant="h6" gutterBottom>Serial port</Typography>
          <TextField
            select
            fullWidth
            label="Baud rate"
            value={data.baud_rate}
            onChange={(e) => setField('baud_rate')(Number(e.target.value))}
            sx={{ mb: 2 }}
          >
            {BAUDRATES.map((r) => <MenuItem key={r} value={r}>{r}</MenuItem>)}
          </TextField>

          <FormControl component="fieldset" sx={{ mb: 2 }}>
            <FormLabel>Data bits</FormLabel>
            <RadioGroup row value={String(data.data_bits)} onChange={(e) => setField('data_bits')(Number(e.target.value))}>
              {[5, 6, 7, 8].map((n) => <FormControlLabel key={n} value={String(n)} control={<Radio />} label={String(n)} />)}
            </RadioGroup>
          </FormControl>

          <FormControl component="fieldset" sx={{ mb: 2 }}>
            <FormLabel>Stop bits</FormLabel>
            <RadioGroup row value={String(data.stop_bits)} onChange={(e) => setField('stop_bits')(Number(e.target.value))}>
              <FormControlLabel value="1" control={<Radio />} label="1" />
              <FormControlLabel value="2" control={<Radio />} label="2" />
            </RadioGroup>
          </FormControl>

          <FormControl component="fieldset" sx={{ mb: 2 }}>
            <FormLabel>Parity</FormLabel>
            <RadioGroup row value={String(data.parity)} onChange={(e) => setField('parity')(Number(e.target.value))}>
              <FormControlLabel value="0" control={<Radio />} label="None" />
              <FormControlLabel value="1" control={<Radio />} label="Even" />
              <FormControlLabel value="2" control={<Radio />} label="Odd" />
            </RadioGroup>
          </FormControl>

          <TextField
            select
            fullWidth
            label="Line ending"
            value={data.line_ending}
            onChange={(e) => setField('line_ending')(Number(e.target.value) as any)}
          >
            <MenuItem value={0}>None</MenuItem>
            <MenuItem value={1}>{'CR (\\r)'}</MenuItem>
            <MenuItem value={2}>{'LF (\\n)'}</MenuItem>
            <MenuItem value={3}>{'CRLF (\\r\\n)'}</MenuItem>
          </TextField>

          <ButtonRow mt={2}>
            <Button startIcon={<SaveIcon />} variant="contained" disabled={saving} onClick={saveData}>Save serial port</Button>
          </ButtonRow>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterSettings;
