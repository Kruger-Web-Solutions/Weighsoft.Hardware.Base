import { FC, useCallback, useContext, useEffect, useRef, useState } from 'react';

import { Alert, Box, Button, TextField, Typography } from '@mui/material';

import { FormLoader, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';
import { useWs } from '../../utils';

import {
  deleteLiveWeightProduct,
  LiveWeightProductEntry,
  liveWeightReportUrl,
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

/** Offline / explicit demo only — never seed LIVE UI with this list. */
const DEMO_CATALOG: LiveWeightProductEntry[] = [
  { plu: '19', product: 'Screw M6', unit: 'kg' },
  { plu: '21', product: 'Washer', unit: 'kg' }
];

const EMPTY_DRAFT = { plu: '', product: '', count: 1, unit: 'kg' };

type CatalogSource = 'none' | 'loading' | 'live' | 'demo' | 'error';

const LiveWeightProduct: FC = () => {
  const { me } = useContext(AuthenticationContext);
  const isLoggedIn = !!me;
  const { connected, data, updateData } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [local, setLocal] = useState<LiveWeightState>(DEMO_LIVE_WEIGHT);
  // Empty until board WS / catalog — never flash Screw M6 as "live" active PLU.
  const [draft, setDraft] = useState(EMPTY_DRAFT);
  // Empty until board load or explicit demo — do not flash DEMO_CATALOG on remount.
  const [catalog, setCatalog] = useState<LiveWeightProductEntry[]>([]);
  const [catalogSource, setCatalogSource] = useState<CatalogSource>('none');
  const [txCount, setTxCount] = useState(0);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<string | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const liveLoadSucceeded = useRef(false);
  const pluSyncedRef = useRef(false);
  const boardStateRef = useRef<LiveWeightState | undefined>(undefined);

  useEffect(() => {
    if (!connected) {
      const t = window.setTimeout(() => setDemoMode(true), 2500);
      return () => window.clearTimeout(t);
    }
    setDemoMode(false);
    return undefined;
  }, [connected]);

  // Prefer board WS; never treat DEMO_LIVE_WEIGHT as live active PLU while waiting.
  const state = demoMode ? local : data;
  const boardOnline = connected && !demoMode;
  boardStateRef.current = data;

  const statePlu = state?.plu;
  const stateProduct = state?.product;
  const stateCount = state?.count;
  const stateUnit = state?.unit;

  useEffect(() => {
    if (statePlu === undefined && stateProduct === undefined) {
      return;
    }
    setDraft({
      plu: statePlu || '',
      product: stateProduct || '',
      count: stateCount ?? 1,
      unit: stateUnit || 'kg'
    });
  }, [statePlu, stateProduct, stateCount, stateUnit, demoMode, connected]);

  const syncActiveFromCatalog = useCallback(async (products: LiveWeightProductEntry[]) => {
    if (!products.length || pluSyncedRef.current) {
      return;
    }
    const board = boardStateRef.current;
    // Wait for live WS state so we do not overwrite a valid board PLU with catalog[0].
    if (!board) {
      return;
    }
    const boardPlu = (board.plu || '').trim();
    const inCatalog = boardPlu !== '' && products.some((p) => p.plu === boardPlu);
    if (inCatalog) {
      const match = products.find((p) => p.plu === boardPlu)!;
      setDraft((prev) => ({
        ...prev,
        plu: match.plu,
        product: match.product,
        unit: match.unit || prev.unit || 'kg',
        count: board.count ?? prev.count
      }));
      pluSyncedRef.current = true;
      return;
    }
    // Board PLU missing / stale demo — select first catalog entry (public select).
    const first = products[0];
    try {
      await selectLiveWeightProduct(first.plu);
      setDraft((prev) => ({
        ...prev,
        plu: first.plu,
        product: first.product,
        unit: first.unit || 'kg',
        count: board.count ?? prev.count
      }));
      setMessage(`Active PLU synced: ${first.plu}`);
    } catch {
      setDraft((prev) => ({
        ...prev,
        plu: first.plu,
        product: first.product,
        unit: first.unit || 'kg'
      }));
      setMessage(`Showing catalog PLU ${first.plu} (select may have failed)`);
    }
    pluSyncedRef.current = true;
  }, []);

  const refreshCatalog = useCallback(async () => {
    setCatalogSource((prev) => (prev === 'live' ? 'live' : 'loading'));
    setLoadError(null);
    try {
      const [productsRes, txRes] = await Promise.all([
        readLiveWeightProducts(),
        readLiveWeightTransactions()
      ]);
      const products = productsRes.data.products || [];
      setCatalog(products);
      setTxCount(txRes.data.count ?? 0);
      setCatalogSource('live');
      liveLoadSucceeded.current = true;
      await syncActiveFromCatalog(products);
    } catch (err: unknown) {
      const status = (err as { response?: { status?: number; data?: { error?: string } } })?.response;
      const detail =
        status?.data?.error ||
        (status?.status ? `HTTP ${status.status}` : 'Could not load catalog from board');
      setLoadError(detail);
      // Keep last good catalog — do not wipe to empty/demo
      setCatalogSource(liveLoadSucceeded.current ? 'live' : 'error');
      setMessage(
        liveLoadSucceeded.current
          ? `Catalog refresh failed (showing last list): ${detail}`
          : `Catalog load failed: ${detail}`
      );
    }
  }, [syncActiveFromCatalog]);

  useEffect(() => {
    if (boardOnline) {
      pluSyncedRef.current = false;
      void refreshCatalog();
      return;
    }
    if (demoMode) {
      // Never wipe a successful live list with demo on remount / flicker.
      if (liveLoadSucceeded.current) {
        return;
      }
      setCatalog(DEMO_CATALOG);
      setTxCount(0);
      setCatalogSource('demo');
      setLoadError(null);
      setDraft({
        plu: DEMO_LIVE_WEIGHT.plu,
        product: DEMO_LIVE_WEIGHT.product,
        count: DEMO_LIVE_WEIGHT.count,
        unit: DEMO_LIVE_WEIGHT.unit
      });
    }
  }, [boardOnline, demoMode, refreshCatalog]);

  // When WS state arrives after catalog, re-check PLU membership once.
  useEffect(() => {
    if (boardOnline && catalogSource === 'live' && catalog.length > 0 && data && !pluSyncedRef.current) {
      void syncActiveFromCatalog(catalog);
    }
  }, [boardOnline, catalog, catalogSource, data, syncActiveFromCatalog]);

  const saveActive = async () => {
    const payload = {
      plu: draft.plu,
      product: draft.product,
      count: Number(draft.count) || 1,
      unit: draft.unit || 'kg'
    };

    if (demoMode && !boardOnline) {
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

    if (demoMode && !boardOnline) {
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
      setCatalogSource('demo');
      setMessage('Saved to catalog (demo)');
      return;
    }

    setSaving(true);
    setMessage(null);
    try {
      const res = await upsertLiveWeightProduct(entry);
      setCatalog(res.data.products || []);
      setCatalogSource('live');
      liveLoadSucceeded.current = true;
      setLoadError(null);
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
    if (demoMode && !boardOnline) {
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
    if (demoMode && !boardOnline) {
      setCatalog((prev) => prev.filter((p) => p.plu !== plu));
      setMessage('Removed (demo)');
      return;
    }
    setSaving(true);
    setMessage(null);
    try {
      const res = await deleteLiveWeightProduct(plu);
      setCatalog(res.data.products || []);
      setCatalogSource('live');
      liveLoadSucceeded.current = true;
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

  const totalDisplay = `${state?.total || state?.weight || '—'} ${state?.unit || draft.unit || 'kg'}`;

  const showLiveBanner = catalogSource === 'live' || (boardOnline && catalogSource === 'loading');
  const showDemoBanner = catalogSource === 'demo' || (demoMode && !liveLoadSucceeded.current && !boardOnline);

  return (
    <SectionContent title="Product" titleGutter>
      {showLiveBanner && (
        <Alert severity="success" sx={{ mb: 2 }}>
          <strong>LIVE</strong> — product catalog from the board. Leave this tab and come back; the list stays.
        </Alert>
      )}
      {showDemoBanner && (
        <Alert severity="warning" sx={{ mb: 2 }}>
          <strong>DEMO</strong> — board offline. Catalog is sample data in the browser only (not saved on the board).
        </Alert>
      )}
      {loadError && boardOnline && (
        <Alert
          severity="error"
          sx={{ mb: 2 }}
          action={
            <Button color="inherit" size="small" onClick={() => void refreshCatalog()}>
              Retry
            </Button>
          }
        >
          Catalog load failed: {loadError}
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
                value={totalDisplay}
                InputProps={{ readOnly: true }}
                helperText="Derived on the board (count × weight)"
              />
            </Box>

            <div className="lw-actions">
              <Button variant="contained" onClick={saveActive} disabled={saving}>
                Save active
              </Button>
              {isLoggedIn && (
                <Button
                  variant="outlined"
                  onClick={saveToCatalog}
                  disabled={
                    saving ||
                    (boardOnline && catalog.length >= MAX_PRODUCTS && !catalog.some((p) => p.plu === draft.plu.trim()))
                  }
                >
                  Save to catalog
                </Button>
              )}
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
            {catalogSource === 'live' ? ' · LIVE' : catalogSource === 'demo' ? ' · DEMO' : ''}
          </div>
          <div className="lw-card-body">
            <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, flexWrap: 'wrap' }}>
              <Button
                variant="contained"
                color="primary"
                size="small"
                component="a"
                href={liveWeightReportUrl()}
                disabled={txCount === 0}
              >
                Download report (CSV)
              </Button>
              <Typography variant="body2" color="text.secondary">
                {txCount === 0
                  ? 'No weighs recorded yet — press Next or Print to record one.'
                  : `${txCount} weigh${txCount === 1 ? '' : 's'} saved on the board. Opens in Excel.`}
              </Typography>
            </Box>
          </div>
          <div className="lw-card-body">
            {catalogSource === 'loading' && catalog.length === 0 ? (
              <Typography variant="body2" color="text.secondary">
                Loading catalog from board…
              </Typography>
            ) : catalog.length === 0 ? (
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
                      {isLoggedIn && (
                        <Button size="small" color="error" onClick={() => removeProduct(entry.plu)} disabled={saving}>
                          Delete
                        </Button>
                      )}
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
