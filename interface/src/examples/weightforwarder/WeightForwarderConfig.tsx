import { FC, useContext } from 'react';
import {
  Box,
  Button,
  Checkbox,
  FormControl,
  FormControlLabel,
  IconButton,
  InputLabel,
  MenuItem,
  Select,
  TextField,
  Typography,
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import AddIcon from '@mui/icons-material/Add';
import DeleteIcon from '@mui/icons-material/Delete';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { FeaturesContext } from '../../contexts/features';
import { useRest } from '../../utils';
import { updateWeightForwarder, readWeightForwarder } from '../../api/weightForwarder';
import {
  ForwardProtocol,
  OutputFormat,
  type WeightForwarderData,
  HEARTBEAT_DEFAULT_SEC,
  HEARTBEAT_MIN_SEC,
  HEARTBEAT_MAX_SEC,
} from '../../types/weightForwarder';

const OUTPUT_FORMAT_OPTIONS = [
  { value: OutputFormat.STANDARD, label: 'Standard JSON' },
  { value: OutputFormat.LCD, label: 'LCD Display (16-char)' },
  { value: OutputFormat.TFT, label: 'TFT Display (3.5")' },
  { value: OutputFormat.SERIAL, label: 'Serial Output (TX)' },
];

const WeightForwarderConfig: FC = () => {
  const { features } = useContext(FeaturesContext);
  const {
    data,
    loadData,
    saveData,
    saving,
    setData,
    errorMessage,
  } = useRest<WeightForwarderData>({ read: readWeightForwarder, update: updateWeightForwarder });

  const setField = (key: keyof WeightForwarderData) => (value: any) => {
    setData((prev) => (prev ? { ...prev, [key]: value } : prev));
  };

  const setHeartbeatSec = (raw: string) => {
    if (raw === '') {
      setField('heartbeat_interval_sec')(0);
      return;
    }
    const parsed = Number(raw);
    if (!Number.isFinite(parsed)) return;
    const intVal = Math.floor(parsed);
    if (intVal <= 0) {
      setField('heartbeat_interval_sec')(0);
      return;
    }
    const clamped = Math.min(HEARTBEAT_MAX_SEC, Math.max(HEARTBEAT_MIN_SEC, intVal));
    setField('heartbeat_interval_sec')(clamped);
  };

  const getUrls = (): string[] =>
    data?.target_urls?.length ? data.target_urls : (data?.target_url ? [data.target_url] : ['']);

  const getFormats = (): OutputFormat[] =>
    data?.target_formats?.length ? data.target_formats : getUrls().map(() => OutputFormat.STANDARD);

  const setUrl = (idx: number, value: string) => {
    const urls = [...getUrls()];
    urls[idx] = value;
    setData((prev) => prev ? { ...prev, target_urls: urls, target_url: urls[0] ?? '' } : prev);
  };

  const setFormat = (idx: number, value: OutputFormat) => {
    const fmts = [...getFormats()];
    fmts[idx] = value;
    setData((prev) => prev ? { ...prev, target_formats: fmts } : prev);
  };

  const addUrl = () => {
    if (getUrls().length >= 5) return;
    const urls = [...getUrls(), ''];
    const fmts = [...getFormats(), OutputFormat.STANDARD];
    setData((prev) => prev ? { ...prev, target_urls: urls, target_formats: fmts, target_url: urls[0] ?? '' } : prev);
  };

  const removeUrl = (idx: number) => {
    const urls = getUrls().filter((_, i) => i !== idx);
    const fmts = getFormats().filter((_, i) => i !== idx);
    setData((prev) => prev ? {
      ...prev,
      target_urls: urls.length ? urls : [''],
      target_formats: fmts.length ? fmts : [OutputFormat.STANDARD],
      target_url: urls[0] ?? ''
    } : prev);
  };

  if (!data) {
    return (
      <SectionContent title="Configuration" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const getAvailableProtocols = () => {
    const protocols = [
      { value: ForwardProtocol.HTTP, label: 'HTTP POST' },
      { value: ForwardProtocol.WS, label: 'WebSocket Client' },
    ];
    if (features.mqtt) {
      protocols.push({ value: ForwardProtocol.MQTT, label: 'MQTT' });
    }
    if (features.ble) {
      protocols.push({ value: ForwardProtocol.BLE, label: 'BLE Client' });
    }
    return protocols;
  };

  return (
    <SectionContent title="Weight Stream Forwarder Configuration" titleGutter>
      <Box display="flex" flexDirection="column" gap={3}>
        <FormControlLabel
          control={
            <Checkbox
              checked={data.enabled}
              onChange={(e) => setField('enabled')(e.target.checked)}
            />
          }
          label="Enable Weight Forwarding"
        />

        <FormControlLabel
          control={
            <Checkbox
              checked={data.usb_echo_enabled ?? false}
              onChange={(e) => setField('usb_echo_enabled')(e.target.checked)}
            />
          }
          label="Echo scale line to USB serial port"
          title="When this ESP is connected to a PC via USB, every captured scale line is written to the COM port. Independent of network forwarding."
        />

        <TextField
          fullWidth
          size="small"
          type="number"
          label="Heartbeat interval (seconds)"
          value={data.heartbeat_interval_sec ?? HEARTBEAT_DEFAULT_SEC}
          onChange={(e) => setHeartbeatSec(e.target.value)}
          inputProps={{ min: 0, max: HEARTBEAT_MAX_SEC, step: 1 }}
          helperText={
            `Resends the latest weight every N seconds even if it hasn't changed. ` +
            `Default ${HEARTBEAT_DEFAULT_SEC}s. ` +
            `0 = disabled (only forward on weight change). ` +
            `Range when enabled: ${HEARTBEAT_MIN_SEC}\u2013${HEARTBEAT_MAX_SEC}s. ` +
            `Weight changes are always forwarded immediately (subject to a 0.5s safety floor).`
          }
        />

        <FormControl fullWidth>
          <InputLabel>Protocol</InputLabel>
          <Select
            value={data.protocol}
            onChange={(e) => setField('protocol')(e.target.value as ForwardProtocol)}
            label="Protocol"
          >
            {getAvailableProtocols().map((proto) => (
              <MenuItem key={proto.value} value={proto.value}>
                {proto.label}
              </MenuItem>
            ))}
          </Select>
        </FormControl>

        {data.protocol === ForwardProtocol.HTTP && (
          <>
            <Box>
              <Typography variant="subtitle2" sx={{ mb: 1, color: 'text.secondary' }}>
                Targets (max 5) — weight data is sent to ALL targets
              </Typography>
              {getUrls().map((url, idx) => (
                <Box key={idx} display="flex" alignItems="flex-start" gap={1} sx={{ mb: 1.5 }}>
                  <TextField
                    sx={{ flex: 2 }}
                    size="small"
                    label={`Target ${idx + 1} URL`}
                    value={url}
                    onChange={(e) => setUrl(idx, e.target.value)}
                    placeholder="http://192.168.1.50/rest/remoteWeight"
                    disabled={getFormats()[idx] === OutputFormat.SERIAL}
                    helperText={getFormats()[idx] === OutputFormat.SERIAL ? 'No URL needed for serial output' : undefined}
                  />
                  <FormControl sx={{ minWidth: 160 }} size="small">
                    <InputLabel>Format</InputLabel>
                    <Select
                      value={getFormats()[idx] ?? OutputFormat.STANDARD}
                      onChange={(e) => setFormat(idx, e.target.value as OutputFormat)}
                      label="Format"
                    >
                      {OUTPUT_FORMAT_OPTIONS.map((opt) => (
                        <MenuItem key={opt.value} value={opt.value}>{opt.label}</MenuItem>
                      ))}
                    </Select>
                  </FormControl>
                  {getUrls().length > 1 && (
                    <IconButton onClick={() => removeUrl(idx)} color="error" size="small" title="Remove" sx={{ mt: 0.5 }}>
                      <DeleteIcon fontSize="small" />
                    </IconButton>
                  )}
                </Box>
              ))}
              {getUrls().length < 5 && (
                <Button startIcon={<AddIcon />} size="small" onClick={addUrl} variant="outlined">
                  Add Target
                </Button>
              )}
            </Box>
            <Box sx={{ mt: 1 }}>
              <TextField
                fullWidth
                size="small"
                label="Target login (username)"
                type="text"
                autoComplete="username"
                value={data.auth_username ?? ''}
                onChange={(e) => setField('auth_username')(e.target.value)}
                placeholder="Leave empty if target does not require login"
                helperText="Used for all HTTP targets (e.g. admin)"
                sx={{ mb: 1 }}
              />
              <TextField
                fullWidth
                size="small"
                label="Target login (password)"
                type="password"
                autoComplete="current-password"
                value={data.auth_password ?? ''}
                onChange={(e) => setField('auth_password')(e.target.value)}
                placeholder="Password for target device"
              />
            </Box>
          </>
        )}

        {data.protocol === ForwardProtocol.WS && (
          <>
          <TextField
            fullWidth
            label="WebSocket URL"
            value={data.ws_url}
            onChange={(e) => setField('ws_url')(e.target.value)}
            placeholder="ws://192.168.3.100/ws/display"
            helperText="WebSocket endpoint for real-time streaming"
          />
          <Box sx={{ mt: 1 }}>
            <TextField
              fullWidth
              size="small"
              label="Target login (username)"
              type="text"
              autoComplete="username"
              value={data.auth_username ?? ''}
              onChange={(e) => setField('auth_username')(e.target.value)}
              placeholder="Leave empty if target does not require login"
              helperText="Required when target requires login (token appended as ?access_token=)"
              sx={{ mb: 1 }}
            />
            <TextField
              fullWidth
              size="small"
              label="Target login (password)"
              type="password"
              autoComplete="current-password"
              value={data.auth_password ?? ''}
              onChange={(e) => setField('auth_password')(e.target.value)}
              placeholder="Password for target device"
            />
          </Box>
          </>
        )}

        {data.protocol === ForwardProtocol.MQTT && features.mqtt && (
          <TextField
            fullWidth
            label="MQTT Topic"
            value={data.mqtt_topic}
            onChange={(e) => setField('mqtt_topic')(e.target.value)}
            placeholder="remote/device/weight"
            helperText="MQTT topic to publish weight data"
          />
        )}

        {data.protocol === ForwardProtocol.BLE && features.ble && (
          <>
            <TextField
              fullWidth
              label="BLE Service UUID"
              value={data.ble_service_uuid}
              onChange={(e) => setField('ble_service_uuid')(e.target.value)}
              placeholder="12340000-e8f2-537e-4f6c-d104768a1234"
              helperText="Remote BLE service UUID"
            />
            <TextField
              fullWidth
              label="BLE Characteristic UUID"
              value={data.ble_char_uuid}
              onChange={(e) => setField('ble_char_uuid')(e.target.value)}
              placeholder="12340001-e8f2-537e-4f6c-d104768a1234"
              helperText="Remote BLE characteristic UUID for writing weight data"
            />
          </>
        )}

        <ButtonRow>
          <Button
            startIcon={<SaveIcon />}
            variant="contained"
            color="primary"
            onClick={saveData}
            disabled={saving}
          >
            Save Configuration
          </Button>
        </ButtonRow>
      </Box>
    </SectionContent>
  );
};

export default WeightForwarderConfig;
