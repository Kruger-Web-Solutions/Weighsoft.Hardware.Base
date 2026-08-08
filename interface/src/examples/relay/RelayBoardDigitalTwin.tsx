import { FC, useCallback, useEffect, useState } from 'react';

import { Alert, Box, Chip, FormControlLabel, Switch, Typography } from '@mui/material';

import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { readSystemStatus } from '../../api/system';
import { FormLoader, SectionContent } from '../../components';
import { SystemStatus } from '../../types';
import { useWs } from '../../utils';

import { LiveWeightState } from '../liveweight/types';
import { readRelayBoardStatus } from './api';
import RelayBoardTwin3D from './RelayBoardTwin3D';
import {
  DEMO_BOARD_STATUS,
  DEMO_RELAY_STATE,
  RelayBoardState,
  RelayBoardStatus
} from './types';
import './relayTwin.css';

export const RELAY_BOARD_WEBSOCKET_URL = WEB_SOCKET_ROOT + 'relayBoard';
export const LIVE_WEIGHT_WEBSOCKET_URL = WEB_SOCKET_ROOT + 'liveWeight';

const formatBytes = (n?: number) => {
  if (n == null || Number.isNaN(n)) {
    return '—';
  }
  if (n >= 1048576) {
    return `${(n / 1048576).toFixed(2)} MB`;
  }
  if (n >= 1024) {
    return `${(n / 1024).toFixed(1)} KB`;
  }
  return `${n} B`;
};

const formatUptime = (ms?: number) => {
  if (ms == null || Number.isNaN(ms)) {
    return '—';
  }
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d > 0) {
    return `${d}d ${h}h ${m}m`;
  }
  if (h > 0) {
    return `${h}h ${m}m`;
  }
  return `${m}m ${s % 60}s`;
};

const RelayBoardDigitalTwin: FC = () => {
  const { connected, updateData, data } = useWs<RelayBoardState>(RELAY_BOARD_WEBSOCKET_URL);
  const { data: liveWeight } = useWs<LiveWeightState>(LIVE_WEIGHT_WEBSOCKET_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [weightPulse, setWeightPulse] = useState(false);
  const [localState, setLocalState] = useState<RelayBoardState>(DEMO_RELAY_STATE);
  const [boardStatus, setBoardStatus] = useState<RelayBoardStatus>(DEMO_BOARD_STATUS);
  const [systemStatus, setSystemStatus] = useState<SystemStatus | undefined>();

  useEffect(() => {
    if (!connected) {
      const timer = window.setTimeout(() => setDemoMode(true), 2500);
      return () => window.clearTimeout(timer);
    }
    setDemoMode(false);
    return undefined;
  }, [connected]);

  useEffect(() => {
    if (!liveWeight?.timestamp && !liveWeight?.weight) {
      return;
    }
    setWeightPulse(true);
    const t = window.setTimeout(() => setWeightPulse(false), 1600);
    return () => window.clearTimeout(t);
  }, [liveWeight?.timestamp, liveWeight?.weight]);

  useEffect(() => {
    if (demoMode || !connected) {
      return;
    }
    let cancelled = false;
    const load = async () => {
      try {
        const [boardRes, sysRes] = await Promise.all([
          readRelayBoardStatus(),
          readSystemStatus()
        ]);
        if (!cancelled) {
          setBoardStatus(boardRes.data);
          setSystemStatus(sysRes.data);
        }
      } catch {
        // keep last / demo values
      }
    };
    load();
    const id = window.setInterval(load, 5000);
    return () => {
      cancelled = true;
      window.clearInterval(id);
    };
  }, [connected, demoMode]);

  const state = demoMode ? localState : (data || DEMO_RELAY_STATE);

  const toggleRelay = useCallback((key: keyof Pick<RelayBoardState, 'relay1' | 'relay2' | 'relay3' | 'relay4'>) => {
    if (demoMode) {
      setLocalState((prev) => ({ ...prev, [key]: !prev[key] }));
      return;
    }
    if (!data) {
      return;
    }
    updateData({ ...data, [key]: !data[key] });
  }, [demoMode, data, updateData]);

  const setRelay = (key: keyof RelayBoardState, value: boolean) => {
    if (demoMode) {
      setLocalState((prev) => ({ ...prev, [key]: value }));
      return;
    }
    if (!data) {
      return;
    }
    updateData({ ...data, [key]: value });
  };

  if (!demoMode && (!connected || !data)) {
    return (
      <SectionContent title="Digital Twin" titleGutter>
        <FormLoader message="Connecting to relay board WebSocket… (demo unlocks if offline)" />
      </SectionContent>
    );
  }

  const status = demoMode ? DEMO_BOARD_STATUS : boardStatus;
  const heap = systemStatus?.free_heap ?? status.free_heap;
  const frag = systemStatus && 'heap_fragmentation' in systemStatus
    ? systemStatus.heap_fragmentation
    : status.heap_fragmentation;

  return (
    <SectionContent title="Digital Twin" titleGutter>
      {demoMode && (
        <Alert severity="info" sx={{ mb: 2 }}>
          Demo mode — board offline or WebSocket not connected. Click relays on the isometric twin to fire GPIO pipes.
          Live control uses <code>/ws/relayBoard</code> when the ESP is reachable.
        </Alert>
      )}

      <RelayBoardTwin3D
        state={state}
        powerOn
        uartLive={weightPulse || demoMode}
        hasBuzzer={status.has_buzzer}
        onToggleRelay={toggleRelay}
        onToggleBuzzer={() => setRelay('buzzer', !state.buzzer)}
      />

      <Typography variant="subtitle2" gutterBottom>ESP stats</Typography>
      <div className="relay-stats-grid">
        <div className="relay-stat"><div className="k">MCU</div><div className="v">{status.mcu}</div></div>
        <div className="relay-stat"><div className="k">CPU</div><div className="v">{status.cpu_freq_mhz} MHz</div></div>
        <div className="relay-stat"><div className="k">Free RAM</div><div className="v">{formatBytes(heap)}</div></div>
        <div className="relay-stat"><div className="k">Heap frag</div><div className="v">{frag}%</div></div>
        <div className="relay-stat"><div className="k">Uptime</div><div className="v">{formatUptime(status.uptime_ms)}</div></div>
        <div className="relay-stat">
          <div className="k">Supply VCC</div>
          <div className="v">{status.vcc_mv != null ? `${(status.vcc_mv / 1000).toFixed(2)} V` : '—'}</div>
        </div>
        <div className="relay-stat">
          <div className="k">WiFi signal</div>
          <div className="v">{status.wifi_rssi != null ? `${status.wifi_rssi} dBm` : '—'}</div>
        </div>
        <div className="relay-stat"><div className="k">IP address</div><div className="v">{status.ip ?? '—'}</div></div>
        <div className="relay-stat"><div className="k">Flash</div><div className="v">{formatBytes(status.flash_chip_size)}</div></div>
        <div className="relay-stat">
          <div className="k">Sketch free</div>
          <div className="v">{formatBytes(status.free_sketch_space)}</div>
        </div>
        <div className="relay-stat"><div className="k">Reset reason</div><div className="v">{status.reset_reason ?? '—'}</div></div>
        <div className="relay-stat"><div className="k">Temp sensor</div><div className="v">None on PCB</div></div>
        <div className="relay-stat"><div className="k">Chip ID</div><div className="v">{status.chip_id}</div></div>
        <div className="relay-stat"><div className="k">MAC</div><div className="v">{status.mac ?? '—'}</div></div>
      </div>

      <Typography variant="subtitle2" gutterBottom>Digital inputs (DI) — live</Typography>
      <Box mb={2}>
        <Chip
          sx={{ mr: 1 }}
          label={`DI 1 · GPIO${status.pins.di1 ?? 4} — ${state.di1 ? 'ACTIVE' : 'idle'} · ${
            liveWeight?.di1_action || 'none'
          }`}
          color={state.di1 ? 'success' : 'default'}
        />
        <Chip
          label={`DI 2 · GPIO${status.pins.di2 ?? 5} — ${state.di2 ? 'ACTIVE' : 'idle'} · ${
            liveWeight?.di2_action || 'none'
          }`}
          color={state.di2 ? 'success' : 'default'}
        />
        {demoMode && (
          <>
            <FormControlLabel
              sx={{ ml: 2 }}
              control={<Switch checked={state.di1} onChange={(_, v) => setRelay('di1', v)} size="small" />}
              label="simulate DI1"
            />
            <FormControlLabel
              control={<Switch checked={state.di2} onChange={(_, v) => setRelay('di2', v)} size="small" />}
              label="simulate DI2"
            />
          </>
        )}
      </Box>

      <Typography variant="subtitle2" gutterBottom>Digital outputs (DO) — relays</Typography>
      <Box mb={2}>
        {([
          ['relay1', 'RY1 GPIO16'],
          ['relay2', 'RY2 GPIO14'],
          ['relay3', 'RY3 GPIO12'],
          ['relay4', 'RY4 GPIO13']
        ] as const).map(([key, label]) => (
          <FormControlLabel
            key={key}
            control={<Switch checked={state[key]} onChange={(_, v) => setRelay(key, v)} color="primary" />}
            label={label}
          />
        ))}
        {status.has_buzzer && (
          <FormControlLabel
            control={<Switch checked={state.buzzer} onChange={(_, v) => setRelay('buzzer', v)} color="secondary" />}
            label="Buzzer"
          />
        )}
      </Box>

      <Typography variant="subtitle2" gutterBottom>GPIO map (DI / DO / boot)</Typography>
      <Box mb={2}>
        {Object.entries(status.gpio_legend).map(([gpio, role]) => {
          const isDi = role.startsWith('DI');
          const pinNum = Number(gpio);
          const on =
            (pinNum === status.pins.ry1 && state.relay1) ||
            (pinNum === status.pins.ry2 && state.relay2) ||
            (pinNum === status.pins.ry3 && state.relay3) ||
            (pinNum === status.pins.ry4 && state.relay4) ||
            (pinNum === status.pins.buzzer && state.buzzer) ||
            (pinNum === (status.pins.di1 ?? 4) && isDi && state.di1) ||
            (pinNum === (status.pins.di2 ?? 5) && isDi && state.di2);
          const boot = role.toLowerCase().includes('boot');
          return (
            <span key={gpio} className={`gpio-chip ${on ? 'do-on' : ''} ${boot ? 'boot' : ''}`}>
              <Chip size="small" label={`GPIO${gpio}`} color={on ? 'success' : 'default'} />
              {role}
            </span>
          );
        })}
      </Box>

      <Typography variant="body2" color="textSecondary">
        Power LED is hardwired. Relay LEDs follow coil drive (active {status.relay_active}).
        UART flash path: Laptop → Prolific USB-Serial → MAX3232 → ESP TX0/RX0/GND (IO0 low to flash).
      </Typography>
    </SectionContent>
  );
};

export default RelayBoardDigitalTwin;
