import { FC, useEffect, useState } from 'react';

import { Alert, Box, Button, TextField, Typography } from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import { updateLiveWeight } from './api';
import './liveWeight.css';
import { LIVE_WEIGHT_WS_URL } from './LiveWeightScreen';
import { DEMO_LIVE_WEIGHT, LiveWeightState } from './types';

const LiveWeightProduct: FC = () => {
  const { connected, data, updateData } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [local, setLocal] = useState<LiveWeightState>(DEMO_LIVE_WEIGHT);
  const [draft, setDraft] = useState({
    plu: DEMO_LIVE_WEIGHT.plu,
    product: DEMO_LIVE_WEIGHT.product,
    count: DEMO_LIVE_WEIGHT.count,
    unit: DEMO_LIVE_WEIGHT.unit
  });
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

  useEffect(() => {
    setDraft({
      plu: state.plu || '',
      product: state.product || '',
      count: state.count ?? 1,
      unit: state.unit || 'kg'
    });
  }, [state.plu, state.product, state.count, state.unit, demoMode, connected]);

  const save = async () => {
    const payload = {
      plu: draft.plu,
      product: draft.product,
      count: Number(draft.count) || 1,
      unit: draft.unit || 'kg'
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

  if (!demoMode && !data && !connected) {
    return (
      <SectionContent title="Product" titleGutter>
        <FormLoader message="Connecting…" />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Product" titleGutter>
      {demoMode && (
        <Alert severity="info" sx={{ mb: 2 }}>
          Demo mode — product changes stay in the browser until the board is online.
        </Alert>
      )}

      <Box className="lw-page">
        <div className="lw-card">
          <div className="lw-card-head">PLU / piece count</div>
          <div className="lw-card-body">
            <Box sx={{ display: 'grid', gap: 2, gridTemplateColumns: { xs: '1fr', sm: '1fr 1fr' } }}>
              <TextField
                size="small"
                label="PLU"
                value={draft.plu}
                disabled={saving}
                onChange={(e) => setDraft((prev) => ({ ...prev, plu: e.target.value }))}
              />
              <TextField
                size="small"
                label="Description"
                value={draft.product}
                disabled={saving}
                onChange={(e) => setDraft((prev) => ({ ...prev, product: e.target.value }))}
              />
              <TextField
                size="small"
                label="Count"
                type="number"
                value={draft.count}
                disabled={saving}
                onChange={(e) => setDraft((prev) => ({ ...prev, count: Number(e.target.value) || 0 }))}
              />
              <TextField
                size="small"
                label="Unit"
                value={draft.unit}
                disabled={saving}
                onChange={(e) => setDraft((prev) => ({ ...prev, unit: e.target.value }))}
              />
              <TextField
                size="small"
                label="Total"
                value={`${state.total || state.weight || '—'} ${state.unit || 'kg'}`}
                InputProps={{ readOnly: true }}
                helperText="Derived on the board (count × weight)"
              />
            </Box>

            <div className="lw-actions">
              <Button variant="contained" onClick={save} disabled={saving}>
                Save
              </Button>
              {message && (
                <Typography variant="body2" color="text.secondary">
                  {message}
                </Typography>
              )}
            </div>
          </div>
        </div>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightProduct;
