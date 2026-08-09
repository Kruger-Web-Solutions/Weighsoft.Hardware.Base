import { FC, useEffect, useRef, useState } from 'react';
import { Link as RouterLink } from 'react-router-dom';

import {
  Alert,
  Box,
  Button,
  Chip,
  FormControl,
  InputLabel,
  Link,
  MenuItem,
  Select,
  TextField,
  Typography
} from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { readWiFiSettings, readWiFiStatus } from '../../api/wifi';
import { useWs } from '../../utils';
import { WiFiConnectionStatus } from '../../types';

import { updateLiveWeight } from './api';
import './liveWeight.css';
import { LIVE_WEIGHT_WS_URL } from './LiveWeightScreen';
import { DEMO_LIVE_WEIGHT, LiveWeightSourceId, LiveWeightState, SOURCE_OPTIONS } from './types';

/** Admin Tech tab: weight input path (serial / WiFi). RS-485 is not available on this board. */
const LiveWeightSetup: FC = () => {
  const { connected, data, updateData } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [local, setLocal] = useState<LiveWeightState>(DEMO_LIVE_WEIGHT);
  const [streaming, setStreaming] = useState(false);
  const [history, setHistory] = useState<Array<{ weight: string; line: string; time: Date; source: string }>>([]);
  const [saving, setSaving] = useState(false);
  const [testWeight, setTestWeight] = useState('1.50');
  const [baudDraft, setBaudDraft] = useState<number | null>(null);
  const [regexDraft, setRegexDraft] = useState<string | null>(null);
  const [boardIp, setBoardIp] = useState<string>('');
  const [boardHost, setBoardHost] = useState<string>('');
  const scrollRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    let cancelled = false;
    const loadIdentity = async () => {
      try {
        const [statusRes, settingsRes] = await Promise.all([readWiFiStatus(), readWiFiSettings()]);
        if (cancelled) {
          return;
        }
        const status = statusRes.data;
        if (status.status === WiFiConnectionStatus.WIFI_STATUS_CONNECTED && status.local_ip) {
          setBoardIp(status.local_ip);
        }
        if (settingsRes.data?.hostname) {
          setBoardHost(settingsRes.data.hostname);
        }
      } catch {
        // Tech page still usable without WiFi identity
      }
    };
    void loadIdentity();
    const t = window.setInterval(() => void loadIdentity(), 15000);
    return () => {
      cancelled = true;
      window.clearInterval(t);
    };
  }, []);

  useEffect(() => {
    if (!connected) {
      const t = window.setTimeout(() => setDemoMode(true), 2500);
      return () => window.clearTimeout(t);
    }
    setDemoMode(false);
    return undefined;
  }, [connected]);

  const state = demoMode ? local : data || DEMO_LIVE_WEIGHT;

  useEffect(() => {
    if (!streaming) {
      return;
    }
    if (!state?.last_line && !state?.weight) {
      return;
    }
    if (!state.timestamp) {
      return;
    }
    setHistory((prev) => {
      const last = prev[prev.length - 1];
      if (last && last.line === state.last_line && last.weight === state.weight) {
        return prev;
      }
      return [
        ...prev,
        {
          weight: state.weight || '—',
          line: state.last_line || '',
          time: new Date(),
          source: state.active_source || state.source_name
        }
      ].slice(-200);
    });
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [streaming, state?.timestamp, state?.weight, state?.last_line, state?.active_source, state?.source_name]);

  const applyConfig = async (patch: Partial<LiveWeightState>) => {
    if (demoMode) {
      setLocal((prev) => ({ ...prev, ...patch }));
      return;
    }
    setSaving(true);
    try {
      const res = await updateLiveWeight({ ...state, ...patch });
      updateData(res.data);
    } finally {
      setSaving(false);
    }
  };

  const sendTestWeight = async () => {
    const payload = { weight: testWeight, last_line: `TEST,${testWeight}` };
    if (demoMode) {
      setLocal((prev) => ({
        ...prev,
        ...payload,
        timestamp: Date.now(),
        active_source: 'wifi',
        status_message: 'Demo test weight'
      }));
      return;
    }
    setSaving(true);
    try {
      const res = await updateLiveWeight(payload);
      updateData(res.data);
    } finally {
      setSaving(false);
    }
  };

  if (!demoMode && !data && !connected) {
    return (
      <SectionContent title="Tech" titleGutter>
        <FormLoader />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Tech" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Technician page: how weight comes into the board. Everyday settings live on{' '}
        <strong>Target & Relays</strong> and <strong>Product</strong>. RS-485 is not available on this board.
      </Alert>

      <Alert severity="warning" sx={{ mb: 2 }}>
        <strong>Printer IP and port are not on Tech.</strong> Set them on{' '}
        <Link component={RouterLink} to="/live-weight/target#network-printer" underline="always" fontWeight={700}>
          Target & Relays → Network printer
        </Link>
        .
      </Alert>

      <Box className="lw-page">
        <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 1, mb: 2, alignItems: 'center' }}>
          <Chip size="small" label={demoMode ? 'Demo' : connected ? 'WS connected' : 'Disconnected'} color="success" />
          <Chip size="small" variant="outlined" label={`Zone: ${state.zone_name || 'none'}`} />
          <Chip size="small" variant="outlined" label={`Weight: ${state.weight || '—'}`} />
          <Chip
            size="small"
            color={streaming ? 'primary' : 'default'}
            label={streaming ? 'Stream ON' : 'Stream OFF'}
          />
          <Button
            size="small"
            variant="outlined"
            component={RouterLink}
            to="/live-weight/target#network-printer"
          >
            Printer IP & port → Target & Relays
          </Button>
        </Box>

        <div className="lw-card" id="how-senders-find-me">
          <div className="lw-card-head">How senders find me</div>
          <div className="lw-card-body">
            <Typography variant="body2" paragraph>
              This board <strong>announces</strong> on the LAN so any sender can auto-find it. Weight is still{' '}
              <strong>pushed to</strong> the board. Manual IP is set on the <strong>sender</strong>, not here.
            </Typography>
            <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 1, mb: 1 }}>
              <Chip size="small" color="primary" label={`IP: ${boardIp || '—'}`} />
              <Chip size="small" variant="outlined" label={`Hostname: ${boardHost || '—'}`} />
              <Chip size="small" variant="outlined" label="UDP announce :4210" />
              <Chip size="small" variant="outlined" label="mDNS _weighsoft-lw._tcp" />
            </Box>
            <Typography variant="caption" color="text.secondary" component="div">
              Sender listens for service <code>weighsoft-lw</code> on UDP 4210, then POSTs{' '}
              <code>{'{ weight, last_line }'}</code> to <code>/rest/liveWeight</code> (or WebSocket{' '}
              <code>/ws/liveWeight</code>). Full protocol: docs/WIFI-WEIGHT-DISCOVERY.md
            </Typography>
          </div>
        </div>

        <div className="lw-card">
          <div className="lw-card-head">Weight input source</div>
          <div className="lw-card-body">
            <FormControl fullWidth size="small">
              <InputLabel id="lw-source">Input source</InputLabel>
              <Select
                labelId="lw-source"
                label="Input source"
                value={state.source === 2 ? 0 : state.source}
                disabled={saving}
                onChange={(e) => applyConfig({ source: Number(e.target.value) as LiveWeightSourceId })}
              >
                {SOURCE_OPTIONS.map((o) => (
                  <MenuItem key={o.value} value={o.value} disabled={!!o.disabled}>
                    {o.label}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
            <Typography variant="caption" color="text.secondary">
              {SOURCE_OPTIONS.find((o) => o.value === (state.source === 2 ? 0 : state.source))?.help}
            </Typography>

            {state.source === 0 && (
              <Box sx={{ display: 'grid', gap: 2, gridTemplateColumns: { xs: '1fr', sm: '1fr 1fr' } }}>
                <TextField
                  size="small"
                  label="Baud rate"
                  type="number"
                  value={baudDraft ?? state.baud_rate}
                  onChange={(e) => setBaudDraft(Number(e.target.value) || 9600)}
                  onBlur={() => {
                    const baud = baudDraft ?? state.baud_rate;
                    setBaudDraft(null);
                    applyConfig({ baud_rate: baud });
                  }}
                />
                <TextField
                  size="small"
                  label="Weight regex (optional)"
                  value={regexDraft ?? state.regex_pattern}
                  onChange={(e) => setRegexDraft(e.target.value)}
                  onBlur={() => {
                    const regex = regexDraft ?? state.regex_pattern;
                    setRegexDraft(null);
                    applyConfig({ regex_pattern: regex });
                  }}
                  helperText="Leave default for fast simple parse"
                />
              </Box>
            )}

            <Box sx={{ display: 'flex', gap: 1, alignItems: 'center', flexWrap: 'wrap' }}>
              <TextField
                size="small"
                label="Test weight"
                value={testWeight}
                onChange={(e) => setTestWeight(e.target.value)}
                sx={{ width: 140 }}
              />
              <Button variant="contained" onClick={sendTestWeight} disabled={saving}>
                Send test weight
              </Button>
            </Box>
          </div>
        </div>

        <div className="lw-card">
          <div className="lw-card-head">Live stream</div>
          <div className="lw-card-body">
            <Typography variant="body2" color="text.secondary">
              Press <strong>Connect</strong> to append incoming weight / serial lines below. <strong>Stop</strong> freezes
              the box (WebSocket stays up for config).
            </Typography>
            <Box sx={{ display: 'flex', gap: 1, flexWrap: 'wrap' }}>
              {!streaming ? (
                <Button
                  variant="contained"
                  color="primary"
                  onClick={() => setStreaming(true)}
                  disabled={!demoMode && !connected}
                >
                  Connect
                </Button>
              ) : (
                <Button variant="contained" color="secondary" onClick={() => setStreaming(false)}>
                  Stop
                </Button>
              )}
              <Button size="small" variant="outlined" onClick={() => setHistory([])}>
                Clear box
              </Button>
            </Box>
            <Box
              ref={scrollRef}
              sx={{
                fontFamily: 'IBM Plex Mono, Consolas, monospace',
                fontSize: '0.85rem',
                bgcolor: streaming ? 'rgba(11, 61, 102, 0.04)' : 'background.paper',
                border: '2px solid',
                borderColor: streaming ? 'primary.main' : 'divider',
                p: 2,
                borderRadius: 1,
                height: 260,
                overflow: 'auto'
              }}
            >
              {!streaming && history.length === 0 ? (
                <Typography color="text.secondary">Stream stopped — press Connect to capture lines.</Typography>
              ) : history.length === 0 ? (
                <Typography color="text.secondary">Connected — waiting for weight / serial data…</Typography>
              ) : (
                history.map((entry, idx) => (
                  <Box key={`${entry.time.getTime()}-${idx}`} sx={{ mb: 0.5 }}>
                    <Typography component="span" variant="caption" color="text.secondary" sx={{ mr: 1 }}>
                      [{entry.time.toLocaleTimeString()}]
                    </Typography>
                    <Typography component="span" color="primary" sx={{ mr: 1, fontWeight: 700 }}>
                      {entry.weight}
                    </Typography>
                    <Typography component="span" sx={{ mr: 1 }} color="text.secondary">
                      {entry.source}
                    </Typography>
                    <Typography component="span">{entry.line}</Typography>
                  </Box>
                ))
              )}
            </Box>
          </div>
        </div>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightSetup;
