import { FC, useEffect, useState } from 'react';

import {
  Alert,
  Box,
  Button,
  FormControl,
  FormControlLabel,
  InputLabel,
  MenuItem,
  Select,
  Switch,
  TextField,
  Typography
} from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import { updateLiveWeight } from './api';
import './liveWeight.css';
import { LIVE_WEIGHT_WS_URL } from './LiveWeightScreen';
import {
  DEMO_LIVE_WEIGHT,
  DI_ACTION_OPTIONS,
  DiActionId,
  LiveWeightState,
  RELAY_OPTIONS
} from './types';

const LiveWeightTarget: FC = () => {
  const { connected, data, updateData } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [local, setLocal] = useState<LiveWeightState>(DEMO_LIVE_WEIGHT);
  const [draft, setDraft] = useState<LiveWeightState>(DEMO_LIVE_WEIGHT);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<string | null>(null);

  useEffect(() => {
    if (!connected) {
      const t = window.setTimeout(() => setDemoMode(true), 2500);
      return () => window.clearTimeout(t);
    }
    setDemoMode(false);
    return undefined;
  }, [connected]);

  const state = demoMode ? local : data || DEMO_LIVE_WEIGHT;
  const {
    range_enabled,
    range_low,
    range_high,
    relay_low,
    relay_ok,
    relay_high,
    di1_action,
    di2_action,
    printer_enabled,
    printer_ip,
    printer_port
  } = state;

  useEffect(() => {
    setDraft((prev) => ({
      ...prev,
      range_enabled,
      range_low,
      range_high,
      relay_low,
      relay_ok,
      relay_high,
      di1_action,
      di2_action,
      printer_enabled,
      printer_ip,
      printer_port
    }));
  }, [
    range_enabled,
    range_low,
    range_high,
    relay_low,
    relay_ok,
    relay_high,
    di1_action,
    di2_action,
    printer_enabled,
    printer_ip,
    printer_port
  ]);

  const patchDraft = (patch: Partial<LiveWeightState>) => {
    setDraft((prev) => ({ ...prev, ...patch }));
  };

  const save = async () => {
    const payload: Partial<LiveWeightState> = {
      range_enabled: draft.range_enabled,
      range_low: Number(draft.range_low),
      range_high: Number(draft.range_high),
      relay_low: Number(draft.relay_low),
      relay_ok: Number(draft.relay_ok),
      relay_high: Number(draft.relay_high),
      di1_action: draft.di1_action,
      di2_action: draft.di2_action,
      printer_enabled: draft.printer_enabled,
      printer_ip: draft.printer_ip || '',
      printer_port: Number(draft.printer_port) || 9100
    };

    if (demoMode) {
      setLocal((prev) => ({ ...prev, ...payload }));
      setMessage('Saved (demo)');
      return;
    }

    setSaving(true);
    setMessage(null);
    try {
      const res = await updateLiveWeight(payload);
      updateData(res.data);
      setMessage('Saved');
    } catch {
      setMessage('Save failed');
    } finally {
      setSaving(false);
    }
  };

  const triggerAction = async (action: DiActionId) => {
    if (demoMode) {
      setLocal((prev) => ({
        ...prev,
        last_action: action,
        action_seq: (prev.action_seq || 0) + 1,
        count: action === 'next' ? (prev.count || 0) + 1 : prev.count,
        job_running: action === 'start' ? true : action === 'stop' ? false : prev.job_running,
        status_message: `Demo ${action}`
      }));
      setMessage(`Triggered ${action} (demo)`);
      return;
    }
    setSaving(true);
    setMessage(null);
    try {
      const res = await updateLiveWeight({ trigger_action: action });
      updateData(res.data);
      setMessage(`Triggered ${action}`);
    } catch {
      setMessage(`Trigger ${action} failed`);
    } finally {
      setSaving(false);
    }
  };

  if (!demoMode && !data && !connected) {
    return (
      <SectionContent title="Target & Relays" titleGutter>
        <FormLoader message="Connecting…" />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Target & Relays" titleGutter>
      {demoMode && (
        <Alert severity="info" sx={{ mb: 2 }}>
          Demo mode — changes stay in the browser until the board is online.
        </Alert>
      )}

      <Box className="lw-page">
        <div className="lw-card">
          <div className="lw-card-head">Target range</div>
          <div className="lw-card-body">
            <FormControlLabel
              control={
                <Switch
                  checked={!!draft.range_enabled}
                  disabled={saving}
                  onChange={(e) => patchDraft({ range_enabled: e.target.checked })}
                />
              }
              label="Enable range control (drives UNDER / CORRECT / OVER relays)"
            />
            <Box sx={{ display: 'grid', gap: 2, gridTemplateColumns: { xs: '1fr', sm: '1fr 1fr' } }}>
              <TextField
                size="small"
                label="Low limit"
                type="number"
                value={draft.range_low}
                disabled={saving}
                onChange={(e) => patchDraft({ range_low: Number(e.target.value) })}
              />
              <TextField
                size="small"
                label="High limit"
                type="number"
                value={draft.range_high}
                disabled={saving}
                onChange={(e) => patchDraft({ range_high: Number(e.target.value) })}
              />
            </Box>
          </div>
        </div>

        <div className="lw-card lw-card-printer" id="network-printer">
          <div className="lw-card-head">Network printer (ESC/POS)</div>
          <div className="lw-card-body">
            <Alert severity="info" sx={{ mb: 1.5 }}>
              Set the printer LAN IP here. Default port is <strong>9100</strong> (raw TCP). Used by DI Print and Test
              Print.
            </Alert>
            <FormControlLabel
              control={
                <Switch
                  checked={!!draft.printer_enabled}
                  disabled={saving}
                  onChange={(e) => patchDraft({ printer_enabled: e.target.checked })}
                />
              }
              label="Enable network print"
            />
            <Box sx={{ display: 'grid', gap: 2, gridTemplateColumns: { xs: '1fr', sm: '2fr 1fr' }, mt: 1 }}>
              <TextField
                size="small"
                label="Printer IP address"
                value={draft.printer_ip || ''}
                disabled={saving}
                placeholder="e.g. 192.168.2.50"
                helperText="Same WiFi / LAN as this board"
                onChange={(e) => patchDraft({ printer_ip: e.target.value })}
              />
              <TextField
                size="small"
                label="TCP port"
                type="number"
                value={draft.printer_port ?? 9100}
                disabled={saving}
                helperText="Usually 9100"
                onChange={(e) => patchDraft({ printer_port: Number(e.target.value) || 9100 })}
              />
            </Box>
          </div>
        </div>

        <div className="lw-card">
          <div className="lw-card-head">Band → relays</div>
          <div className="lw-card-body">
            <Box sx={{ display: 'grid', gap: 2, gridTemplateColumns: { xs: '1fr', sm: '1fr 1fr 1fr' } }}>
              <FormControl size="small" fullWidth>
                <InputLabel>UNDER → relay</InputLabel>
                <Select
                  label="UNDER → relay"
                  value={draft.relay_low}
                  disabled={saving}
                  onChange={(e) => patchDraft({ relay_low: Number(e.target.value) })}
                >
                  {RELAY_OPTIONS.map((o) => (
                    <MenuItem key={o.value} value={o.value}>
                      {o.label}
                    </MenuItem>
                  ))}
                </Select>
              </FormControl>
              <FormControl size="small" fullWidth>
                <InputLabel>CORRECT → relay</InputLabel>
                <Select
                  label="CORRECT → relay"
                  value={draft.relay_ok}
                  disabled={saving}
                  onChange={(e) => patchDraft({ relay_ok: Number(e.target.value) })}
                >
                  {RELAY_OPTIONS.map((o) => (
                    <MenuItem key={o.value} value={o.value}>
                      {o.label}
                    </MenuItem>
                  ))}
                </Select>
              </FormControl>
              <FormControl size="small" fullWidth>
                <InputLabel>OVER → relay</InputLabel>
                <Select
                  label="OVER → relay"
                  value={draft.relay_high}
                  disabled={saving}
                  onChange={(e) => patchDraft({ relay_high: Number(e.target.value) })}
                >
                  {RELAY_OPTIONS.map((o) => (
                    <MenuItem key={o.value} value={o.value}>
                      {o.label}
                    </MenuItem>
                  ))}
                </Select>
              </FormControl>
            </Box>
          </div>
        </div>

        <div className="lw-card">
          <div className="lw-card-head">DI actions</div>
          <div className="lw-card-body">
            <Typography variant="body2" color="text.secondary">
              Rising edge on DI1 / DI2 (contact to GND) runs the selected action once.
            </Typography>
            <Box sx={{ display: 'grid', gap: 2, gridTemplateColumns: { xs: '1fr', sm: '1fr 1fr' } }}>
              <FormControl size="small" fullWidth>
                <InputLabel>DI1 action</InputLabel>
                <Select
                  label="DI1 action"
                  value={draft.di1_action || 'none'}
                  disabled={saving}
                  onChange={(e) => patchDraft({ di1_action: e.target.value as DiActionId })}
                >
                  {DI_ACTION_OPTIONS.map((o) => (
                    <MenuItem key={o.value} value={o.value}>
                      {o.label}
                    </MenuItem>
                  ))}
                </Select>
              </FormControl>
              <FormControl size="small" fullWidth>
                <InputLabel>DI2 action</InputLabel>
                <Select
                  label="DI2 action"
                  value={draft.di2_action || 'none'}
                  disabled={saving}
                  onChange={(e) => patchDraft({ di2_action: e.target.value as DiActionId })}
                >
                  {DI_ACTION_OPTIONS.map((o) => (
                    <MenuItem key={o.value} value={o.value}>
                      {o.label}
                    </MenuItem>
                  ))}
                </Select>
              </FormControl>
            </Box>
          </div>
        </div>

        <div className="lw-card">
          <div className="lw-card-head">Tests</div>
          <div className="lw-card-body">
            <Typography variant="body2" color="text.secondary">
              Print / Next call Live Weight <code>trigger_action</code>.
            </Typography>
            <div className="lw-actions">
              <Button variant="outlined" disabled={saving} onClick={() => triggerAction('print')}>
                Test Print
              </Button>
              <Button variant="outlined" disabled={saving} onClick={() => triggerAction('next')}>
                Test Next
              </Button>
            </div>
          </div>
        </div>

        <div className="lw-actions" style={{ marginBottom: 24 }}>
          <Button variant="contained" onClick={save} disabled={saving}>
            Save
          </Button>
          {message && (
            <Typography variant="body2" color="text.secondary">
              {message}
            </Typography>
          )}
        </div>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightTarget;
