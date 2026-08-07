import { FC, useEffect, useMemo, useState } from 'react';

import { RelayBoardState } from './types';
import './relayTwin.css';

type RelayKey = 'relay1' | 'relay2' | 'relay3' | 'relay4';

interface RelayBoardTwin3DProps {
  state: RelayBoardState;
  powerOn?: boolean;
  uartLive?: boolean;
  onToggleRelay: (key: RelayKey) => void;
}

interface Tip {
  title: string;
  body: string;
}

const TIPS: Record<string, Tip> = {
  ac: {
    title: 'AC L / N input',
    body: 'Mains feed into the onboard supply (transformer / rectifier). Powers the whole board.'
  },
  psu: {
    title: 'Power section',
    body: 'LM2596 buck → 5V for relays; AMS1117 → 3.3V for ESP-12F. Orange packets = power rail flow.'
  },
  esp: {
    title: 'ESP-12F (DOIT)',
    body: 'WiFi MCU. Runs firmware, drives RY1–RY4 GPIOs, UART TX0/RX0 for flash/debug.'
  },
  uart: {
    title: 'UART / flash header',
    body: 'TX0, RX0, GND, 3V3, IO0. Laptop → Prolific → MAX3232 → here. Blue packets = serial traffic.'
  },
  max: {
    title: 'MAX3232 adapter',
    body: 'RS-232 ↔ TTL level shifter between USB-serial and ESP UART pins.'
  },
  pwrled: {
    title: 'Power LED',
    body: 'Hardwired red status LED near the TXD/GND/5V terminal — not a GPIO.'
  },
  relay1: {
    title: 'RY1 · Songle SRD-05VDC',
    body: 'GPIO16 → driver → coil. Click to toggle. Green packets fire when the DO signal is ON.'
  },
  relay2: {
    title: 'RY2 · Songle SRD-05VDC',
    body: 'GPIO14 → driver → coil. COM / NO / NC on the screw terminal below.'
  },
  relay3: {
    title: 'RY3 · Songle SRD-05VDC',
    body: 'GPIO12 → driver → coil. Active LOW on typical LC-style boards.'
  },
  relay4: {
    title: 'RY4 · Songle SRD-05VDC',
    body: 'GPIO13 → driver → coil. 10A class contacts for switched loads.'
  },
  term: {
    title: 'Load terminals',
    body: 'Per relay: Common, Normally Open, Normally Closed screw terminals.'
  }
};

const RELAYS: { key: RelayKey; gpio: number; x: number }[] = [
  { key: 'relay1', gpio: 16, x: 170 },
  { key: 'relay2', gpio: 14, x: 290 },
  { key: 'relay3', gpio: 12, x: 410 },
  { key: 'relay4', gpio: 13, x: 530 }
];

/** Isometric helpers: map board (u,v) to screen */
const iso = (u: number, v: number, z = 0) => {
  const x = 460 + (u - v) * 0.86;
  const y = 70 + (u + v) * 0.48 - z;
  return { x, y };
};

const Pipe: FC<{
  d: string;
  active: boolean;
  kind?: 'gpio' | 'uart' | 'pwr';
  packets?: number;
  duration?: string;
}> = ({ d, active, kind = 'gpio', packets = 3, duration = '1.4s' }) => {
  const cls = `pipe pipe-${kind} ${active ? 'pipe-active' : ''}`;
  return (
    <g className={cls}>
      <path className="pipe pipe-base" d={d} />
      <path className="pipe pipe-glow" d={d} />
      {Array.from({ length: packets }).map((_, i) => (
        <circle key={i} className="packet" r={4.2} cx={0} cy={0}>
          {active && (
            <animateMotion
              dur={duration}
              begin={`${i * 0.35}s`}
              repeatCount="indefinite"
              path={d}
            />
          )}
        </circle>
      ))}
    </g>
  );
};

const RelayBoardTwin3D: FC<RelayBoardTwin3DProps> = ({
  state,
  powerOn = true,
  uartLive = false,
  onToggleRelay
}) => {
  const [tip, setTip] = useState<Tip | null>(null);
  const [uartBurst, setUartBurst] = useState(false);

  // Pulse UART pipe briefly whenever live flag toggles / stays connected
  useEffect(() => {
    if (!uartLive) {
      setUartBurst(false);
      return undefined;
    }
    setUartBurst(true);
    const id = window.setInterval(() => {
      setUartBurst(true);
      window.setTimeout(() => setUartBurst(false), 900);
    }, 2400);
    return () => window.clearInterval(id);
  }, [uartLive]);

  const pEsp = useMemo(() => iso(210, 40, 28), []);
  const pAc = useMemo(() => iso(20, 20, 12), []);
  const pPsu = useMemo(() => iso(70, 90, 18), []);
  const pUart = useMemo(() => iso(280, 20, 20), []);
  const pMax = useMemo(() => iso(340, -40, 16), []);
  const pLed = useMemo(() => iso(150, 10, 10), []);

  const showTip = (id: string) => setTip(TIPS[id] || null);

  return (
    <div className="relay-iso-wrap">
      <div className="relay-iso-stage">
        {tip && (
          <div className="relay-iso-tip">
            <strong>{tip.title}</strong>
            {tip.body}
          </div>
        )}

        <svg
          className="relay-iso-svg"
          viewBox="0 0 920 520"
          role="img"
          aria-label="Isometric digital twin of ESP-12F 4-channel relay board"
        >
          <defs>
            <linearGradient id="pcbGrad" x1="0" y1="0" x2="1" y2="1">
              <stop offset="0%" stopColor="#1c7a44" />
              <stop offset="55%" stopColor="#0f5230" />
              <stop offset="100%" stopColor="#0a3a22" />
            </linearGradient>
            <linearGradient id="espMetal" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor="#c5ced8" />
              <stop offset="45%" stopColor="#7a8490" />
              <stop offset="100%" stopColor="#3e4650" />
            </linearGradient>
            <linearGradient id="relayBlue" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor="#4aa0ff" />
              <stop offset="100%" stopColor="#1a5cb0" />
            </linearGradient>
            <filter id="softShadow" x="-20%" y="-20%" width="140%" height="140%">
              <feDropShadow dx="0" dy="10" stdDeviation="8" floodOpacity="0.45" />
            </filter>
          </defs>

          {/* PCB slab (isometric parallelogram) */}
          <g filter="url(#softShadow)">
            <polygon
              points="120,140 780,80 860,300 200,380"
              fill="url(#pcbGrad)"
              stroke="#8fd9a8"
              strokeOpacity="0.35"
              strokeWidth="2"
            />
            <polygon points="200,380 860,300 860,318 200,398" fill="#062816" />
            <polygon points="780,80 860,300 860,318 780,98" fill="#0a4024" />
          </g>

          {/* Silk grid */}
          <g opacity="0.12" stroke="#c8ffd8" strokeWidth="1">
            {Array.from({ length: 12 }).map((_, i) => {
              const a = iso(20 + i * 22, 10);
              const b = iso(20 + i * 22, 200);
              return <line key={`v${i}`} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
            })}
            {Array.from({ length: 9 }).map((_, i) => {
              const a = iso(20, 10 + i * 22);
              const b = iso(280, 10 + i * 22);
              return <line key={`h${i}`} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
            })}
          </g>

          {/* Power rail pipe: AC → PSU → ESP */}
          <Pipe
            kind="pwr"
            active={!!powerOn}
            duration="2.2s"
            packets={4}
            d={`M ${pAc.x + 20} ${pAc.y + 10} L ${pPsu.x} ${pPsu.y} L ${pEsp.x - 10} ${pEsp.y + 20}`}
          />

          {/* UART pipe: MAX3232 → header → ESP */}
          <Pipe
            kind="uart"
            active={uartBurst || uartLive}
            duration="1.6s"
            packets={4}
            d={`M ${pMax.x} ${pMax.y + 8} L ${pUart.x + 10} ${pUart.y + 30} L ${pEsp.x + 55} ${pEsp.y + 10}`}
          />

          {/* GPIO pipes: ESP → each relay */}
          {RELAYS.map((r) => {
            const ry = iso(r.x / 3.2, 175, 8);
            const d = `M ${pEsp.x + 8} ${pEsp.y + 40} C ${pEsp.x - 20} ${pEsp.y + 90}, ${ry.x} ${ry.y - 50}, ${ry.x} ${ry.y - 8}`;
            return (
              <Pipe
                key={r.key}
                kind="gpio"
                active={state[r.key]}
                duration="1.25s"
                packets={3}
                d={d}
              />
            );
          })}

          {/* AC terminal */}
          <g
            className="hit"
            onMouseEnter={() => showTip('ac')}
            onMouseLeave={() => setTip(null)}
          >
            <rect x={pAc.x - 28} y={pAc.y - 14} width="56" height="28" rx="3" fill="#2f8a4e" stroke="#0d3a1e" />
            <circle cx={pAc.x - 12} cy={pAc.y} r="4" fill="#ddd" />
            <circle cx={pAc.x + 12} cy={pAc.y} r="4" fill="#ddd" />
            <text className="comp-label" x={pAc.x - 22} y={pAc.y - 22}>AC IN</text>
            <text className="comp-sub" x={pAc.x - 26} y={pAc.y + 36}>L / N mains</text>
          </g>

          {/* PSU block */}
          <g
            className="hit"
            onMouseEnter={() => showTip('psu')}
            onMouseLeave={() => setTip(null)}
          >
            <rect x={pPsu.x - 40} y={pPsu.y - 28} width="90" height="56" rx="6" fill="#2a2a2a" stroke="#666" />
            <rect x={pPsu.x - 28} y={pPsu.y - 18} width="28" height="36" rx="2" fill="#c9a227" />
            <circle cx={pPsu.x + 22} cy={pPsu.y} r="12" fill="#111" stroke="#444" />
            <text className="comp-label" x={pPsu.x - 36} y={pPsu.y - 38}>PSU</text>
            <text className="comp-sub" x={pPsu.x - 40} y={pPsu.y + 46}>LM2596 · AMS1117</text>
          </g>

          {/* Power LED */}
          <g
            className="hit"
            onMouseEnter={() => showTip('pwrled')}
            onMouseLeave={() => setTip(null)}
          >
            <circle
              className={powerOn ? 'led-on' : 'led-off'}
              cx={pLed.x}
              cy={pLed.y}
              r="7"
            />
            <text className="comp-label" x={pLed.x + 12} y={pLed.y + 4}>PWR LED</text>
          </g>

          {/* ESP-12F */}
          <g
            className="hit"
            onMouseEnter={() => showTip('esp')}
            onMouseLeave={() => setTip(null)}
          >
            <rect
              x={pEsp.x - 48}
              y={pEsp.y - 34}
              width="110"
              height="78"
              rx="6"
              fill="url(#espMetal)"
              stroke="#e8eef5"
              strokeOpacity="0.35"
            />
            <rect x={pEsp.x - 40} y={pEsp.y - 26} width="70" height="48" rx="3" fill="#1c2228" />
            <path
              d={
                `M ${pEsp.x + 34} ${pEsp.y - 26} ` +
                `L ${pEsp.x + 52} ${pEsp.y - 10} ` +
                `L ${pEsp.x + 52} ${pEsp.y + 10} ` +
                `L ${pEsp.x + 34} ${pEsp.y + 22} Z`
              }
              fill="#d4af37"
              opacity="0.85"
            />
            <text className="comp-label" x={pEsp.x - 40} y={pEsp.y - 44}>ESP-12F</text>
            <text className="comp-sub" x={pEsp.x - 40} y={pEsp.y + 58}>WiFi MCU · GPIO hub</text>
          </g>

          {/* UART header */}
          <g
            className="hit"
            onMouseEnter={() => showTip('uart')}
            onMouseLeave={() => setTip(null)}
          >
            <rect x={pUart.x - 14} y={pUart.y - 8} width="28" height="70" rx="3" fill="#e6dcc0" stroke="#8a7f5a" />
            {[0, 1, 2, 3, 4].map((i) => (
              <circle key={i} cx={pUart.x} cy={pUart.y + 6 + i * 12} r="3" fill="#333" />
            ))}
            <text className="comp-label" x={pUart.x + 20} y={pUart.y + 8}>UART</text>
            <text className="comp-sub" x={pUart.x + 20} y={pUart.y + 22}>TX RX IO0</text>
          </g>

          {/* MAX3232 off-board */}
          <g
            className="hit"
            onMouseEnter={() => showTip('max')}
            onMouseLeave={() => setTip(null)}
          >
            <rect x={pMax.x - 36} y={pMax.y - 22} width="72" height="44" rx="5" fill="#1f6b3a" stroke="#8fd9a8" />
            <rect x={pMax.x - 16} y={pMax.y - 12} width="32" height="24" rx="2" fill="#111" />
            <text className="comp-label" x={pMax.x - 34} y={pMax.y - 30}>MAX3232</text>
            <text className="comp-sub" x={pMax.x - 40} y={pMax.y + 38}>USB-Serial bridge</text>
          </g>

          {/* Relays + load terminals */}
          {RELAYS.map((r) => {
            const p = iso(r.x / 3.2, 175, 10);
            const t = iso(r.x / 3.2, 210, 4);
            const on = state[r.key];
            return (
              <g key={r.key}>
                <g
                  className="hit"
                  onMouseEnter={() => showTip(r.key)}
                  onMouseLeave={() => setTip(null)}
                  onClick={() => onToggleRelay(r.key)}
                >
                  <rect
                    x={p.x - 28}
                    y={p.y - 36}
                    width="56"
                    height="72"
                    rx="4"
                    fill="url(#relayBlue)"
                    stroke={on ? '#9fd4ff' : '#0a3060'}
                    strokeWidth={on ? 2.5 : 1}
                    opacity={on ? 1 : 0.92}
                  />
                  <circle
                    className={on ? 'led-on' : 'led-off'}
                    cx={p.x}
                    cy={p.y - 22}
                    r="5"
                  />
                  <rect x={p.x - 14} y={p.y - 6} width="28" height="22" rx="2" fill="#0d2a55" />
                  <text className="comp-label" x={p.x - 18} y={p.y + 50}>
                    {r.key.replace('relay', 'RY')}
                  </text>
                  <text className="comp-sub" x={p.x - 26} y={p.y + 62}>
                    GPIO{r.gpio}
                  </text>
                </g>
                <g
                  style={{ pointerEvents: 'none' }}
                  onMouseEnter={() => showTip('term')}
                  onMouseLeave={() => setTip(null)}
                >
                  <rect x={t.x - 22} y={t.y - 8} width="44" height="18" rx="2" fill="#2f8a4e" />
                  <circle cx={t.x - 10} cy={t.y + 1} r="2.5" fill="#ddd" />
                  <circle cx={t.x} cy={t.y + 1} r="2.5" fill="#ddd" />
                  <circle cx={t.x + 10} cy={t.y + 1} r="2.5" fill="#ddd" />
                </g>
              </g>
            );
          })}

          <text className="comp-label" x="40" y="36">
            RelayBoardEspBuildIn · isometric twin
          </text>
          <text className="comp-sub" x="40" y="52">
            Hover a part · click a relay · packets = live signal in the pipe
          </text>
        </svg>

        <div className="relay-iso-legend">
          <span><i className="relay-iso-dot pwr" /> Power rail</span>
          <span><i className="relay-iso-dot uart" /> UART / serial</span>
          <span><i className="relay-iso-dot gpio" /> GPIO → relay (fires when ON)</span>
        </div>
      </div>
    </div>
  );
};

export default RelayBoardTwin3D;
