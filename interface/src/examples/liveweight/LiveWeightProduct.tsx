import { FC, useCallback, useEffect, useState } from 'react';

import { Alert, Box, Button, TextField, Typography } from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import {
  deleteLiveWeightProduct,
  LiveWeightProductEntry,
  readLiveWeightProducts,
  readLiveWeightTransactions,
  selectLiveWeightProduct,
  updateLiveWeight,
  upsertLiveWeightProduct
} from './api';
import './liveWeight.css';
import { LIVE_WEIGHT_WS_URL } from './LiveWeightScreen';
import { DEMO_LIVE_WEIGHT, LiveWeightState } from './types';

const MAX_PRODUCTS = 9;
const MAX_TX = 40;

const DEMO_CATALOG: LiveWeightProductEntry[] = [
  { plu: '19', product: 'Screw M6', unit: 'kg' },
  { plu: '21', product: 'Washer', unit: 'kg' }
];

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
  const [catalog, setCatalog] = useState<LiveWeightProductEntry[]>(DEMO_CATALOG);
  const [txCount, setTxCount] = useState(0);
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

  const refreshCatalog = useCallback(async () => {
    if (demoMode) {
      return;
    }
    try {
      const [productsRes, txRes] = await Promise.all([
        readLiveWeightProducts(),
        readLiveWeightTransactions()
      ]);
      setCatalog(productsRes.data.products || []);
      setTxCount(txRes.data.count ?? 0);
    } catch {
      // keep previous catalog
    }
  }, [demoMode]);

  useEffect(() => {
    if (demoMode) {
      setCatalog(DEMO_CATALOG);
      setTxCount(0);
      return;
    }
    if (connected) {
      void refreshCatalog();
    }
  }, [demoMode, connected, refreshCatalog]);

  const saveActive = async () => {
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
      setMessage('Active product saved');
    } catch {
      setMessage('Save failed');
    } finally {
      setSaving(false);
    }
  };

  const saveToCatalog = async () => {
    const entry: LiveWeightProductEntry = {
      plu: draft.plu.trim(),
      product: draft.product.trim(),
      unit: draft.unit || 'kg'
    };
    if (!entry.plu) {
      setMessage('PLU required');
      return;
    }

    if (demoMode) {
      setCatalog((prev) => {
        const idx = prev.findIndex((p) => p.plu === entry.plu);
        if (idx >= 0) {
          const next = [...prev];
          next[idx] = entry;
          return next;
        }
        if (prev.length >= MAX_PRODUCTS) {
          setMessage(`Max ${MAX_PRODUCTS} products`);
          return prev;
        }
        return [...prev, entry];
      });
      setLocal((prev) => ({ ...prev, ...entry, count: draft.count }));
      setMessage('Saved to catalog (demo)');
      return;
    }

    setSaving(true);
    setMessage(null);
    try {
      const res = await upsertLiveWeightProduct(entry);
      setCatalog(res.data.products || []);
      const active = await updateLiveWeight({
        plu: entry.plu,
        product: entry.product,
        unit: entry.unit,
        count: Number(draft.count) || 1
      });
      updateData(active.data);
      setMessage(`Catalog ${res.data.count}/${res.data.max}`);
    } catch (err: unknown) {
      const status = (err as { response?: { status?: number; data?: { error?: string } } })?.response;
      setMessage(status?.data?.error || (status?.status === 400 ? `Max ${MAX_PRODUCTS} products` : 'Catalog save failed'));
    } finally {
      setSaving(false);
    }
  };

  const selectProduct = async (entry: LiveWeightProductEntry) => {
    if (demoMode) {
      setLocal((prev) => ({ ...prev, plu: entry.plu, product: entry.product, unit: entry.unit }));
      setMessage(`Selected ${entry.plu}`);
      return;
    }
    setSaving(true);
    setMessage(null);
    try {
      await selectLiveWeightProduct(entry.plu);
      setDraft((prev) => ({
        ...prev,
        plu: entry.plu,
        product: entry.product,
        unit: entry.unit
      }));
      setMessage(`Active: ${entry.plu}`);
    } catch {
      setMessage('Select failed');
    } finally {
      setSaving(false);
    }
  };

  const removeProduct = async (plu: string) => {
    if (demoMode) {
      setCatalog((prev) => prev.filter((p) => p.plu !== plu));
      setMessage('Removed (demo)');
      return;
    }
    setSaving(true);
    setMessage(null);
    try {
      const res = await deleteLiveWeightProduct(plu);
      setCatalog(res.data.products || []);
      setMessage('Removed');
    } catch {
      setMessage('Delete failed');
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
          Demo mode — catalog stays in the browser until the board is online.
        </Alert>
      )}

      <Box className="lw-page">
        <div className="lw-card">
          <div className="lw-card-head">Active PLU / piece count</div>
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
              <Button variant="contained" onClick={saveActive} disabled={saving}>
                Save active
              </Button>
              <Button
                variant="outlined"
                onClick={saveToCatalog}
                disabled={saving || (!demoMode && catalog.length >= MAX_PRODUCTS && !catalog.some((p) => p.plu === draft.plu.trim()))}
              >
                Save to catalog
              </Button>
              {message && (
                <Typography variant="body2" color="text.secondary">
                  {message}
                </Typography>
              )}
            </div>
          </div>
        </div>

        <div className="lw-card">
          <div className="lw-card-head">
            Catalog ({catalog.length}/{MAX_PRODUCTS}) · Transactions {txCount}/{MAX_TX}
          </div>
          <div className="lw-card-body">
            {catalog.length === 0 ? (
              <Typography variant="body2" color="text.secondary">
                No products yet. Fill PLU + description, then Save to catalog.
              </Typography>
            ) : (
              <Box sx={{ display: 'grid', gap: 1 }}>
                {catalog.map((entry) => (
                  <Box
                    key={entry.plu}
                    sx={{
                      display: 'flex',
                      flexWrap: 'wrap',
                      gap: 1,
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      py: 0.5,
                      borderBottom: '1px solid',
                      borderColor: 'divider'
                    }}
                  >
                    <Typography variant="body2">
                      <strong>{entry.plu}</strong> — {entry.product || '—'} ({entry.unit || 'kg'})
                    </Typography>
                    <Box sx={{ display: 'flex', gap: 1 }}>
                      <Button size="small" onClick={() => selectProduct(entry)} disabled={saving}>
                        Use
                      </Button>
                      <Button size="small" color="error" onClick={() => removeProduct(entry.plu)} disabled={saving}>
                        Delete
                      </Button>
                    </Box>
                  </Box>
                ))}
              </Box>
            )}
          </div>
        </div>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightProduct;
