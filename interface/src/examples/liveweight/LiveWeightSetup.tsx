import { FC, useEffect, useRef, useState } from 'react';

import {
  Alert,
  Box,
  Button,
  Chip,
  FormControl,
  InputLabel,
  MenuItem,
  Select,
  TextField,
  Typography
} from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import { updateLiveWeight } from './api';
import './liveWeight.css';
import { LIVE_WEIGHT_WS_URL } from './LiveWeightScreen';
import { DEMO_LIVE_WEIGHT, LiveWeightSourceId, LiveWeightState, SOURCE_OPTIONS } from './types';

/** Admin Tech tab: weight input path (serial / WiFi). RS-485 is not available on this board. */
const LiveWeightSetup: FC = () => {
  const { connected, data, updateData } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [local, setLocal] = useState<LiveWeightState>(DEMO_LIVE_WEIGHT);
  const [history, setHistory] = useState<Array<{ weight: string; line: string; time: Date; source: string }>>([]);
  const [saving, setSaving] = useState(false);
  const [testWeight, setTestWeight] = useState('1.50');
  const [baudDraft, setBaudDraft] = useState<number | null>(null);
  const [regexDraft, setRegexDraft] = useState<string | null>(null);
  const scrollRef = useRef<HTMLDivElement>(null);

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
      ].slice(-80);
    });
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [state?.timestamp, state?.weight, state?.last_line, state?.active_source, state?.source_name]);

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

      <Box className="lw-page">
        <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 1, mb: 2 }}>
          <Chip size="small" label={demoMode ? 'Demo' : connected ? 'Connected' : 'Disconnected'} color="success" />
          <Chip size="small" variant="outlined" label={`Zone: ${state.zone_name || 'none'}`} />
          <Chip size="small" variant="outlined" label={`Weight: ${state.weight || '—'}`} />
        </Box>

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
                  label="Weight regex"
                  value={regexDraft ?? state.regex_pattern}
                  onChange={(e) => setRegexDraft(e.target.value)}
                  onBlur={() => {
                    const regex = regexDraft ?? state.regex_pattern;
                    setRegexDraft(null);
                    applyConfig({ regex_pattern: regex });
                  }}
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

        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
          <Typography variant="subtitle2">Recent readings</Typography>
          <Button size="small" onClick={() => setHistory([])}>
            Clear
          </Button>
        </Box>
        <Box
          ref={scrollRef}
          sx={{
            fontFamily: 'IBM Plex Mono, Consolas, monospace',
            fontSize: '0.85rem',
            bgcolor: 'background.paper',
            border: '1px solid',
            borderColor: 'divider',
            p: 2,
            borderRadius: 1,
            height: 220,
            overflow: 'auto',
            mb: 2
          }}
        >
          {history.length === 0 ? (
            <Typography color="text.secondary">Waiting for weight data…</Typography>
          ) : (
            history.map((entry, idx) => (
              <Box key={`${entry.time.getTime()}-${idx}`} sx={{ mb: 0.5 }}>
                <Typography component="span" variant="caption" color="text.secondary" sx={{ mr: 1 }}>
                  [{entry.time.toLocaleTimeString()}]
                </Typography>
                <Typography component="span" color="primary" sx={{ mr: 1, fontWeight: 700 }}>
                  {entry.weight}
                </Typography>
                <Typography component="span">{entry.line}</Typography>
              </Box>
            ))
          )}
        </Box>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightSetup;
