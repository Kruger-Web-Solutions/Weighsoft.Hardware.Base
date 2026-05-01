import React, { FC, useEffect } from 'react';
import { useSnackbar } from 'notistack';
import {
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
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest, extractErrorMessage } from '../../utils';
import { readSerialWriter, updateSerialWriter, readWriterSource, updateWriterSource } from '../../api/serialWriter';
import { SerialWriterData, SerialWriterForwarderData } from '../../types/serialWriter';

const BAUDRATES = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400];

const WriterSettings: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({ read: readSerialWriter, update: updateSerialWriter });
  const [src, setSrc] = React.useState<SerialWriterForwarderData | null>(null);
  const [savingSrc, setSavingSrc] = React.useState(false);

  const refreshSrc = React.useCallback(() => {
    readWriterSource().then((r) => setSrc(r.data)).catch(() => {});
  }, []);

  useEffect(() => { refreshSrc(); }, [refreshSrc]);

  if (!data) return <SectionContent title="Settings" titleGutter><FormLoader onRetry={loadData} errorMessage={errorMessage} /></SectionContent>;

  const setField = <K extends keyof SerialWriterData>(k: K) => (v: SerialWriterData[K]) =>
    setData((p) => p ? { ...p, [k]: v } : p);

  const setSrcField = <K extends keyof SerialWriterForwarderData>(k: K) => (v: SerialWriterForwarderData[K]) =>
    setSrc((p) => p ? { ...p, [k]: v } : p);

  const saveSrc = async () => {
    if (!src) return;
    setSavingSrc(true);
    try {
      // Send only the persisted fields; leave runtime fields out of the body
      // so the backend update() doesn't have to ignore stray keys.
      await updateWriterSource({
        source_url: src.source_url,
        connection_method: src.connection_method,
        auto_reconnect: src.auto_reconnect,
        enabled: src.enabled,
      });
      enqueueSnackbar("Source Reader saved", { variant: 'success' });
      // Re-read so the UI reflects what's persisted on the device, including any
      // server-side normalization (e.g. method clamping).
      refreshSrc();
    } catch (error: any) {
      enqueueSnackbar(extractErrorMessage(error, "Failed to save Source Reader"), { variant: 'error' });
    } finally {
      setSavingSrc(false);
    }
  };

  return (
    <SectionContent title="Settings" titleGutter>
      <Alert severity="warning" sx={{ mb: 2 }}>Changing serial port settings restarts the connection.</Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" gutterBottom>Source Reader</Typography>
          {src && (
            <>
              <TextField
                fullWidth
                label="Source Reader URL"
                placeholder="ws://192.168.2.50/ws/serial"
                value={src.source_url}
                onChange={(e) => setSrcField('source_url')(e.target.value)}
                sx={{ mb: 2 }}
              />
              <FormControlLabel
                control={<Switch checked={src.enabled} onChange={(e) => setSrcField('enabled')(e.target.checked)} />}
                label="Enabled"
              />
              <FormControlLabel
                control={<Switch checked={src.auto_reconnect} onChange={(e) => setSrcField('auto_reconnect')(e.target.checked)} />}
                label="Auto-reconnect"
              />
              <ButtonRow mt={1}>
                <Button startIcon={<SaveIcon />} variant="contained" disabled={savingSrc} onClick={saveSrc}>Save source</Button>
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
