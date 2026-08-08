import { FC, useEffect, useMemo, useState } from 'react';

import { Alert, Box } from '@mui/material';

import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import './liveWeight.css';
import { DEMO_LIVE_WEIGHT, LiveWeightState } from './types';

export const LIVE_WEIGHT_WS_URL = WEB_SOCKET_ROOT + 'liveWeight';

const zoneLabel = (zone: number, enabled: boolean) => {
  if (!enabled) {
    return '—';
  }
  if (zone === 1) {
    return 'UNDER';
  }
  if (zone === 2) {
    return 'OK';
  }
  if (zone === 3) {
    return 'OVER';
  }
  return '—';
};

const zoneChipClass = (zone: number, enabled: boolean) => {
  if (!enabled) {
    return 'zone-off';
  }
  if (zone === 1) {
    return 'zone-low';
  }
  if (zone === 2) {
    return 'zone-ok';
  }
  if (zone === 3) {
    return 'zone-high';
  }
  return 'zone-off';
};

const RangeGauge: FC<{ state: LiveWeightState }> = ({ state }) => {
  const low = Number(state.range_low) || 0;
  const high = Number(state.range_high) || 1;
  const span = Math.max(high - low, 0.001);
  const pad = span * 0.75;
  const min = Math.max(0, low - pad);
  const max = high + pad;
  const w = parseFloat(state.weight || '0');
  const t = Math.min(1, Math.max(0, (w - min) / (max - min || 1)));
  const angle = Math.PI - t * Math.PI;
  const cx = 140;
  const cy = 130;
  const r = 100;
  const nx = cx + r * Math.cos(angle);
  const ny = cy - r * Math.sin(angle);

  const lowT = (low - min) / (max - min);
  const highT = (high - min) / (max - min);
  const a0 = Math.PI;
  const aLow = Math.PI - lowT * Math.PI;
  const aHigh = Math.PI - highT * Math.PI;
  const a1 = 0;

  const polar = (a: number, rad = r) => ({ x: cx + rad * Math.cos(a), y: cy - rad * Math.sin(a) });
  const arc = (from: number, to: number, color: string) => {
    const p0 = polar(from);
    const p1 = polar(to);
    const large = from - to > Math.PI ? 1 : 0;
    return (
      <path
        d={`M ${p0.x} ${p0.y} A ${r} ${r} 0 ${large} 1 ${p1.x} ${p1.y}`}
        fill="none"
        stroke={color}
        strokeWidth="22"
        strokeLinecap="butt"
      />
    );
  };

  const labelLow = polar(aLow, r + 28);
  const labelHigh = polar(aHigh, r + 28);

  return (
    <svg className="lw-gauge-svg" viewBox="0 0 280 170" role="img" aria-label="Weight range gauge">
      {arc(a0, aLow, '#f0c419')}
      {arc(aLow, aHigh, '#2ecc71')}
      {arc(aHigh, a1, '#e74c3c')}
      <text x={labelLow.x} y={labelLow.y} textAnchor="middle" fontSize="13" fill="#333">
        {low.toFixed(3)}
      </text>
      <text x={labelHigh.x} y={labelHigh.y} textAnchor="middle" fontSize="13" fill="#333">
        {high.toFixed(3)}
      </text>
      <line x1={cx} y1={cy} x2={nx} y2={ny} stroke="#111" strokeWidth="4" strokeLinecap="round" />
      <circle cx={cx} cy={cy} r="6" fill="#111" />
      <text className="lw-gauge-label" x={cx} y={cy + 36} textAnchor="middle">
        {zoneLabel(state.zone, state.range_enabled)}
      </text>
    </svg>
  );
};

const LiveWeightScreen: FC = () => {
  const { connected, data } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);

  useEffect(() => {
    if (!connected) {
      const t = window.setTimeout(() => setDemoMode(true), 2500);
      return () => window.clearTimeout(t);
    }
    setDemoMode(false);
    return undefined;
  }, [connected]);

  const state = demoMode ? DEMO_LIVE_WEIGHT : data || DEMO_LIVE_WEIGHT;
  const unit = state.unit || 'kg';
  const weight = state.weight || '—';

  const titleBits = useMemo(() => {
    if (!state.range_enabled) {
      return 'Range control off';
    }
    return `Target ${Number(state.range_low).toFixed(3)} – ${Number(state.range_high).toFixed(3)} ${unit}`;
  }, [state.range_enabled, state.range_low, state.range_high, unit]);

  const connLabel = demoMode ? 'Demo' : connected ? 'Connected' : 'Connecting…';
  const connClass = demoMode ? 'demo' : connected ? 'on' : '';

  if (!demoMode && !data && !connected) {
    return (
      <SectionContent title="Live Weight" titleGutter>
        <FormLoader message="Connecting…" />
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Live Weight" titleGutter>
      {demoMode && (
        <Alert severity="info" sx={{ mb: 2 }}>
          Demo mode — board offline. Live dial and product strip are read-only here; edit Target & Relays or Product when
          connected.
        </Alert>
      )}

      <Box className="lw-scale">
        <div className="lw-scale-top">
          <span>Live Weight</span>
          <span className="lw-conn">
            <span className={`lw-conn-dot ${connClass}`} />
            {connLabel}
            <span aria-hidden="true"> · </span>
            {titleBits}
          </span>
        </div>

        <div className="lw-live-body">
          <div className="lw-gauge-hero">
            <RangeGauge state={state} />
          </div>

          <div className="lw-net-hero">
            <div className="lw-net-hero-label">Net</div>
            <div className="lw-net-hero-value">
              {weight}
              <span className="lw-net-hero-unit">{unit}</span>
            </div>
          </div>

          <div className="lw-chips">
            <div className="lw-chip">
              <span>Gross</span>
              <strong>
                {weight} {unit}
              </strong>
            </div>
            <div className={`lw-chip ${zoneChipClass(state.zone, state.range_enabled)}`}>
              <span>Zone</span>
              <strong>{zoneLabel(state.zone, state.range_enabled)}</strong>
            </div>
          </div>
        </div>

        <div className="lw-product-strip">
          <span>PLU {state.plu || '—'}</span>
          <span className="sep">·</span>
          <span>{state.product || '—'}</span>
          <span className="sep">·</span>
          <span>Count {state.count ?? '—'}</span>
          <span className="sep">·</span>
          <span>
            Total {state.total || weight} {unit}
          </span>
        </div>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightScreen;
