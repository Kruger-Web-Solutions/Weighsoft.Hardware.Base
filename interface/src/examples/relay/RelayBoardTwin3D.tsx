import { FC, useEffect, useState } from 'react';

import { RelayBoardState } from './types';
import './relayTwin.css';

type RelayKey = 'relay1' | 'relay2' | 'relay3' | 'relay4';

interface RelayBoardTwin3DProps {
  state: RelayBoardState;
  powerOn?: boolean;
  uartLive?: boolean;
  hasBuzzer?: boolean;
  onToggleRelay: (key: RelayKey) => void;
  onToggleBuzzer?: () => void;
}

interface Tip {
  title: string;
  body: string;
}

const TIPS: Record<string, Tip> = {
  ac: {
    title: 'AC L / N input',
    body: 'Mains feed into the isolation transformer. Powers the whole board.'
  },
  dc: {
    title: 'DC 7–30V input',
    body: 'Alternative DC feed straight into the LM2596 buck converter.'
  },
  trafo: {
    title: 'Transformer + bulk caps',
    body: 'Isolation transformer with electrolytic capacitors smoothing the rails.'
  },
  psu: {
    title: 'LM2596 buck converter',
    body: 'Steps the input down to 5V for the relay coils. Orange packets = power rail flow.'
  },
  ams: {
    title: 'AMS1117 regulator',
    body: '5V → 3.3V linear regulator feeding the ESP-12F.'
  },
  esp: {
    title: 'ESP-12F (DOIT)',
    body: 'WiFi MCU. Drives RY1–RY4, buzzer on GPIO15, reads DI 1/2 on GPIO4/5, UART TX0/RX0 for flash and debug.'
  },
  uart: {
    title: 'UART / flash header',
    body: 'TX0, RX0, GND, 3V3, IO0. Blue packets = serial traffic. Pull IO0 low at reset for flash mode.'
  },
  max: {
    title: 'MAX3232 adapter',
    body: 'RS-232 ↔ TTL level shifter between the USB-serial cable and the ESP UART pins.'
  },
  pwrled: {
    title: 'Power LED',
    body: 'Hardwired red status LED — not on a GPIO.'
  },
  buzzer: {
    title: 'Buzzer · GPIO15',
    body: 'Active-high output. Click to beep on / off. GPIO15 is pulled low at boot so it stays quiet on reset.'
  },
  di: {
    title: 'Digital inputs · GPIO4 / GPIO5',
    body: 'INPUT_PULLUP: close the pin to GND to trigger. The LED here lights live when the input is active.'
  },
  relay1: {
    title: 'RY1 · Songle SRD-05VDC',
    body: 'GPIO16 → driver → coil. Click to toggle. Green packets fire while the DO signal is ON.'
  },
  relay2: {
    title: 'RY2 · Songle SRD-05VDC',
    body: 'GPIO14 → driver → coil. COM / NO / NC on the screw terminal in front.'
  },
  relay3: {
    title: 'RY3 · Songle SRD-05VDC',
    body: 'GPIO12 → driver → coil. Active LOW drive on this board family.'
  },
  relay4: {
    title: 'RY4 · Songle SRD-05VDC',
    body: 'GPIO13 → driver → coil. 10A class contacts for switched loads.'
  }
};

/* ---------- isometric projection ---------- */

const K = 0.95;
const K2 = 0.48;
const OX = 470;
const OY = 96;

const iso = (u: number, v: number, z = 0) => ({ x: OX + (u - v) * K, y: OY + (u + v) * K2 - z });

const pts = (...p: { x: number; y: number }[]) => p.map((q) => `${q.x.toFixed(1)},${q.y.toFixed(1)}`).join(' ');

const shade = (hex: string, f: number) => {
  const n = parseInt(hex.slice(1), 16);
  const r = Math.min(255, Math.round(((n >> 16) & 255) * f));
  const g = Math.min(255, Math.round(((n >> 8) & 255) * f));
  const b = Math.min(255, Math.round((n & 255) * f));
  return `rgb(${r},${g},${b})`;
};

/** Flat path along the board surface through (u,v) waypoints at height z */
const boardPath = (waypoints: [number, number][], z = 3) => {
  return waypoints
    .map(([u, v], i) => {
      const p = iso(u, v, z);
      return `${i === 0 ? 'M' : 'L'} ${p.x.toFixed(1)} ${p.y.toFixed(1)}`;
    })
    .join(' ');
};

/* ---------- primitives ---------- */

const IsoBox: FC<{
  u: number;
  v: number;
  w: number;
  d: number;
  h: number;
  color: string;
  topColor?: string;
  stroke?: string;
  strokeWidth?: number;
}> = ({ u, v, w, d, h, color, topColor, stroke, strokeWidth = 1 }) => {
  const t1 = iso(u, v, h);
  const t2 = iso(u + w, v, h);
  const t3 = iso(u + w, v + d, h);
  const t4 = iso(u, v + d, h);
  const b2 = iso(u + w, v, 0);
  const b3 = iso(u + w, v + d, 0);
  const b4 = iso(u, v + d, 0);
  return (
    <g>
      <polygon points={pts(t4, t3, b3, b4)} fill={shade(color, 0.62)} />
      <polygon points={pts(t3, t2, b2, b3)} fill={shade(color, 0.42)} />
      <polygon points={pts(t1, t2, t3, t4)} fill={topColor || color} stroke={stroke} strokeWidth={strokeWidth} />
    </g>
  );
};

const Cyl: FC<{ u: number; v: number; r: number; h: number; color: string; topColor?: string }> = ({
  u,
  v,
  r,
  h,
  color,
  topColor
}) => {
  const b = iso(u, v, 0);
  const t = iso(u, v, h);
  const ry = r * 0.55;
  const body =
    `M ${b.x - r} ${b.y} L ${t.x - r} ${t.y} ` +
    `A ${r} ${ry} 0 0 1 ${t.x + r} ${t.y} ` +
    `L ${b.x + r} ${b.y} A ${r} ${ry} 0 0 0 ${b.x - r} ${b.y} Z`;
  return (
    <g>
      <path d={body} fill={shade(color, 0.55)} />
      <ellipse cx={t.x} cy={t.y} rx={r} ry={ry} fill={topColor || color} />
    </g>
  );
};

const Pipe: FC<{
  d: string;
  active: boolean;
  kind?: 'gpio' | 'uart' | 'pwr' | 'di';
  packets?: number;
  duration?: string;
}> = ({ d, active, kind = 'gpio', packets = 3, duration = '1.6s' }) => {
  const cls = `pipe pipe-${kind} ${active ? 'pipe-active' : ''}`;
  return (
    <g className={cls}>
      <path className="pipe pipe-base" d={d} />
      <path className="pipe pipe-glow" d={d} />
      {Array.from({ length: packets }).map((_, i) => (
        <circle key={i} className="packet" r={3.6} cx={0} cy={0}>
          {active && (
            <animateMotion dur={duration} begin={`${i * 0.45}s`} repeatCount="indefinite" path={d} />
          )}
        </circle>
      ))}
    </g>
  );
};

/* ---------- layout constants (board units, board = 400 x 230) ---------- */

const RELAYS: { key: RelayKey; gpio: number; u: number }[] = [
  { key: 'relay1', gpio: 16, u: 28 },
  { key: 'relay2', gpio: 14, u: 118 },
  { key: 'relay3', gpio: 12, u: 208 },
  { key: 'relay4', gpio: 13, u: 298 }
];

const RELAY_W = 62;
const RELAY_V = 152;
const RELAY_D = 46;
const RELAY_H = 30;

/* ESP module footprint */
const ESP = { u: 198, v: 18, w: 90, d: 78 };

/* signal start points along the ESP front edge, one per relay */
const relayPipe = (i: number, u: number): string => {
  const sx = 214 + i * 18;
  const corridor = 136 - i * 9;
  const cx = u + RELAY_W / 2;
  return boardPath(
    [
      [sx, ESP.v + ESP.d + 2],
      [sx, corridor],
      [cx, corridor],
      [cx, RELAY_V - 2]
    ],
    3
  );
};

const POWER_PIPE = boardPath(
  [
    [40, 36],
    [40, 118],
    [126, 118],
    [154, 102],
    [196, 62]
  ],
  4
);

const UART_PIPE = boardPath(
  [
    [448, 44],
    [377, 52],
    [292, 52]
  ],
  8
);

const DI_PIPE = boardPath(
  [
    [352, 120],
    [352, 90],
    [292, 90]
  ],
  4
);

const BUZZER_PIPE = boardPath(
  [
    [272, 96],
    [302, 96],
    [302, 112]
  ],
  4
);

/* decorative SMD parts (u, v, w, d, color) */
const SMDS: [number, number, number, number, string][] = [
  [178, 30, 8, 4, '#8a6d3a'],
  [178, 40, 8, 4, '#8a6d3a'],
  [178, 50, 8, 4, '#2a2a2a'],
  [150, 130, 10, 5, '#2a2a2a'],
  [165, 130, 10, 5, '#8a6d3a'],
  [310, 92, 8, 4, '#2a2a2a'],
  [322, 92, 8, 4, '#8a6d3a'],
  [70, 132, 10, 5, '#2a2a2a']
];

/* traces purely for looks */
const DECOR_TRACES: [number, number][][] = [
  [
    [60, 145],
    [340, 145]
  ],
  [
    [20, 200],
    [20, 60]
  ],
  [
    [380, 150],
    [380, 90]
  ]
];

const RelayBoardTwin3D: FC<RelayBoardTwin3DProps> = ({
  state,
  powerOn = true,
  uartLive = false,
  hasBuzzer = true,
  onToggleRelay,
  onToggleBuzzer
}) => {
  const [tip, setTip] = useState<Tip | null>(null);
  const [uartBurst, setUartBurst] = useState(false);

  useEffect(() => {
    if (!uartLive) {
      setUartBurst(false);
      return undefined;
    }
    setUartBurst(true);
    const id = window.setInterval(() => {
      setUartBurst(true);
      window.setTimeout(() => setUartBurst(false), 1100);
    }, 2600);
    return () => window.clearInterval(id);
  }, [uartLive]);

  const showTip = (id: string) => setTip(TIPS[id] || null);
  const hideTip = () => setTip(null);

  /* frequently used anchors */
  const bA = iso(0, 0);
  const bB = iso(400, 0);
  const bC = iso(400, 230);
  const bD = iso(0, 230);
  const thickness = 20;

  const espLabel = iso(ESP.u + 4, ESP.v, 28);
  const pBuzz = iso(302, 126, 14);
  const pLed = iso(174, 64, 6);

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
          viewBox="0 0 1000 480"
          role="img"
          aria-label="Isometric digital twin of ESP-12F 4-channel relay board"
        >
          <defs>
            <linearGradient id="pcbTop" x1="0" y1="0" x2="1" y2="1">
              <stop offset="0%" stopColor="#1b8a4d" />
              <stop offset="50%" stopColor="#127040" />
              <stop offset="100%" stopColor="#0b5530" />
            </linearGradient>
            <linearGradient id="espShield" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor="#d8e0ea" />
              <stop offset="55%" stopColor="#93a0ae" />
              <stop offset="100%" stopColor="#5c6874" />
            </linearGradient>
            <radialGradient id="stageGlow" cx="0.5" cy="0.42" r="0.75">
              <stop offset="0%" stopColor="#1f4a33" stopOpacity="0.9" />
              <stop offset="60%" stopColor="#0e2233" stopOpacity="0.4" />
              <stop offset="100%" stopColor="#000000" stopOpacity="0" />
            </radialGradient>
            <filter id="boardShadow" x="-20%" y="-20%" width="140%" height="160%">
              <feDropShadow dx="0" dy="16" stdDeviation="14" floodOpacity="0.55" />
            </filter>
          </defs>

          <rect x="0" y="0" width="1000" height="480" fill="url(#stageGlow)" />

          {/* ---------- PCB slab ---------- */}
          <g filter="url(#boardShadow)">
            <polygon
              points={pts(
                { x: bD.x, y: bD.y + thickness },
                { x: bC.x, y: bC.y + thickness },
                bC,
                bD
              )}
              fill="#05301a"
            />
            <polygon
              points={pts(
                { x: bC.x, y: bC.y + thickness },
                { x: bB.x, y: bB.y + thickness },
                bB,
                bC
              )}
              fill="#083f22"
            />
            <polygon points={pts(bA, bB, bC, bD)} fill="url(#pcbTop)" stroke="#7fe0a4" strokeOpacity="0.28" />
          </g>

          {/* silkscreen grid */}
          <g opacity="0.09" stroke="#d8ffe6" strokeWidth="1" style={{ pointerEvents: 'none' }}>
            {Array.from({ length: 15 }).map((_, i) => {
              const a = iso(14 + i * 26, 10);
              const b = iso(14 + i * 26, 220);
              return <line key={`v${i}`} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
            })}
            {Array.from({ length: 8 }).map((_, i) => {
              const a = iso(10, 16 + i * 28);
              const b = iso(390, 16 + i * 28);
              return <line key={`h${i}`} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
            })}
          </g>

          {/* decorative copper traces */}
          <g stroke="#e8c26a" strokeOpacity="0.16" strokeWidth="2.5" fill="none" style={{ pointerEvents: 'none' }}>
            {DECOR_TRACES.map((t, i) => (
              <path key={i} d={boardPath(t, 1)} />
            ))}
          </g>

          {/* silk title printed on the board */}
          <text
            transform={`matrix(${K},${K2},${-K},${K2},${iso(24, 210).x},${iso(24, 210).y})`}
            fill="#eafff2"
            opacity="0.3"
            fontFamily="IBM Plex Mono, Consolas, monospace"
            fontSize="13"
            style={{ pointerEvents: 'none' }}
          >
            WEIGHSOFT · RELAY-4CH · ESP-12F
          </text>

          {/* mounting holes */}
          {[iso(14, 14), iso(386, 14), iso(386, 216), iso(14, 216)].map((p, i) => (
            <g key={i} style={{ pointerEvents: 'none' }}>
              <ellipse cx={p.x} cy={p.y} rx="8.5" ry="5" fill="#caa64e" />
              <ellipse cx={p.x} cy={p.y} rx="4.5" ry="2.6" fill="#081720" />
            </g>
          ))}

          {/* ---------- signal pipes (flat on the board) ---------- */}
          <Pipe kind="pwr" active={!!powerOn} duration="2.6s" packets={4} d={POWER_PIPE} />
          <Pipe kind="uart" active={uartBurst || uartLive} duration="1.8s" packets={4} d={UART_PIPE} />
          <Pipe kind="di" active={state.di1} duration="1.3s" packets={2} d={DI_PIPE} />
          <Pipe
            kind="di"
            active={state.di2}
            duration="1.3s"
            packets={2}
            d={boardPath(
              [
                [368, 120],
                [368, 84],
                [292, 84]
              ],
              4
            )}
          />
          {hasBuzzer && <Pipe kind="gpio" active={state.buzzer} duration="1s" packets={2} d={BUZZER_PIPE} />}
          {RELAYS.map((r, i) => (
            <Pipe key={r.key} kind="gpio" active={state[r.key]} duration="1.5s" packets={3} d={relayPipe(i, r.u)} />
          ))}

          {/* ---------- rear row: power inputs ---------- */}
          <g className="hit" onMouseEnter={() => showTip('ac')} onMouseLeave={hideTip}>
            <IsoBox u={12} v={12} w={56} d={26} h={18} color="#2fa05a" stroke="#0d3a1e" />
            {[26, 48].map((u, i) => {
              const p = iso(u, 25, 18);
              return <ellipse key={i} cx={p.x} cy={p.y} rx="4.5" ry="2.6" fill="#e8eef2" />;
            })}
            <text className="comp-label" x={iso(12, 12, 30).x - 24} y={iso(12, 12, 30).y}>AC IN · L/N</text>
          </g>

          <g className="hit" onMouseEnter={() => showTip('dc')} onMouseLeave={hideTip}>
            <IsoBox u={84} v={8} w={62} d={24} h={16} color="#2fa05a" stroke="#0d3a1e" />
            {[96, 114, 132].map((u, i) => {
              const p = iso(u, 20, 16);
              return <ellipse key={i} cx={p.x} cy={p.y} rx="4" ry="2.3" fill="#e8eef2" />;
            })}
            <text className="comp-label" x={iso(96, 8, 26).x} y={iso(96, 8, 26).y}>DC 7–30V</text>
          </g>

          {/* ---------- power section ---------- */}
          <g className="hit" onMouseEnter={() => showTip('trafo')} onMouseLeave={hideTip}>
            <IsoBox u={14} v={64} w={54} d={54} h={26} color="#d9b13b" topColor="#e6c355" stroke="#7a5f10" />
            <IsoBox u={24} v={74} w={34} d={34} h={28} color="#8a6d14" />
            <text className="comp-sub" x={iso(0, 128).x - 8} y={iso(0, 128).y + 12}>transformer</text>
          </g>
          <Cyl u={86} v={74} r={9} h={22} color="#26303c" topColor="#3c4c5e" />
          <Cyl u={102} v={88} r={7.5} h={18} color="#26303c" topColor="#3c4c5e" />

          <g className="hit" onMouseEnter={() => showTip('psu')} onMouseLeave={hideTip}>
            <Cyl u={106} v={118} r={10} h={12} color="#20242c" topColor="#2e3642" />
            <IsoBox u={118} v={106} w={24} d={24} h={9} color="#20242c" topColor="#161a20" />
            <text className="comp-sub" x={iso(96, 140).x - 10} y={iso(96, 140).y + 14}>LM2596 · 5V</text>
          </g>

          <g className="hit" onMouseEnter={() => showTip('ams')} onMouseLeave={hideTip}>
            <IsoBox u={148} v={94} w={16} d={14} h={6} color="#20242c" topColor="#14181e" />
            <text className="comp-sub" x={iso(148, 116).x - 6} y={iso(148, 116).y + 10}>AMS1117</text>
          </g>

          <g className="hit" onMouseEnter={() => showTip('pwrled')} onMouseLeave={hideTip}>
            <circle className={powerOn ? 'led-on' : 'led-off'} cx={pLed.x} cy={pLed.y} r="5.5" />
            <text className="comp-sub" x={pLed.x + 9} y={pLed.y - 4}>PWR</text>
          </g>

          {/* decorative SMDs */}
          <g style={{ pointerEvents: 'none' }}>
            {SMDS.map(([u, v, w, d, c], i) => (
              <IsoBox key={i} u={u} v={v} w={w} d={d} h={3} color={c} />
            ))}
          </g>

          {/* ---------- ESP-12F ---------- */}
          <g className="hit" onMouseEnter={() => showTip('esp')} onMouseLeave={hideTip}>
            <IsoBox u={ESP.u} v={ESP.v} w={ESP.w} d={ESP.d} h={5} color="#10222e" topColor="#132a3a" />
            {/* castellated pads */}
            {Array.from({ length: 8 }).map((_, i) => {
              const p = iso(ESP.u + 8 + i * 11, ESP.v + ESP.d, 3);
              return <rect key={i} x={p.x - 2.5} y={p.y - 2} width="5" height="5" fill="#d8b44a" />;
            })}
            {/* antenna zone */}
            <polygon
              points={pts(
                iso(ESP.u + 4, ESP.v + 4, 6),
                iso(ESP.u + ESP.w - 4, ESP.v + 4, 6),
                iso(ESP.u + ESP.w - 4, ESP.v + 22, 6),
                iso(ESP.u + 4, ESP.v + 22, 6)
              )}
              fill="#0c1c28"
            />
            <path
              d={boardPath(
                [
                  [ESP.u + 8, ESP.v + 8],
                  [ESP.u + 80, ESP.v + 8],
                  [ESP.u + 80, ESP.v + 13],
                  [ESP.u + 12, ESP.v + 13],
                  [ESP.u + 12, ESP.v + 18],
                  [ESP.u + 80, ESP.v + 18]
                ],
                6
              )}
              stroke="#d8b44a"
              strokeWidth="1.6"
              fill="none"
            />
            {/* RF shield */}
            <IsoBox
              u={ESP.u + 10}
              v={ESP.v + 28}
              w={70}
              d={44}
              h={14}
              color="#93a0ae"
              topColor="#c3ccd6"
              stroke="#e8eef4"
              strokeWidth={0.6}
            />
            <text className="comp-label" x={espLabel.x} y={espLabel.y}>ESP-12F · WiFi MCU</text>
          </g>

          {/* ---------- UART header (right rear edge) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('uart')} onMouseLeave={hideTip}>
            <IsoBox u={366} v={28} w={20} d={54} h={12} color="#e6dcc0" topColor="#efe7cf" stroke="#8a7f5a" />
            {[38, 48, 58, 68, 78].map((v, i) => {
              const p = iso(376, v, 12);
              return <circle key={i} cx={p.x} cy={p.y} r="2.2" fill="#20242c" />;
            })}
            <text className="comp-label" x={iso(392, 28, 24).x} y={iso(392, 28, 24).y}>UART</text>
            <text className="comp-sub" x={iso(392, 28, 12).x} y={iso(392, 28, 12).y}>TX RX IO0</text>
          </g>

          {/* ---------- MAX3232 module (off-board) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('max')} onMouseLeave={hideTip}>
            <IsoBox u={428} v={22} w={58} d={42} h={8} color="#17603a" topColor="#1e7546" stroke="#8fd9a8" />
            <IsoBox u={444} v={34} w={26} d={18} h={6} color="#14181e" />
            <text className="comp-label" x={iso(432, 74).x - 4} y={iso(432, 74).y + 18}>MAX3232</text>
            <text className="comp-sub" x={iso(432, 74).x - 4} y={iso(432, 74).y + 31}>USB-serial bridge</text>
          </g>

          {/* ---------- buzzer ---------- */}
          {hasBuzzer && (
            <g
              className="hit"
              onMouseEnter={() => showTip('buzzer')}
              onMouseLeave={hideTip}
              onClick={() => onToggleBuzzer && onToggleBuzzer()}
            >
              {state.buzzer && (
                <>
                  <circle className="buzz-ring" cx={pBuzz.x} cy={pBuzz.y} r="15" />
                  <circle className="buzz-ring buzz-ring-2" cx={pBuzz.x} cy={pBuzz.y} r="21" />
                </>
              )}
              <Cyl u={302} v={126} r={13} h={14} color="#14141c" topColor="#1e1e28" />
              <ellipse cx={pBuzz.x} cy={pBuzz.y} rx="3.4" ry="2" fill={state.buzzer ? '#ffd54a' : '#2e2e3a'} />
              <text className="comp-sub" x={pBuzz.x - 24} y={pBuzz.y - 26}>BUZZER · GPIO15</text>
            </g>
          )}

          {/* ---------- DI header ---------- */}
          <g className="hit" onMouseEnter={() => showTip('di')} onMouseLeave={hideTip}>
            <IsoBox u={340} v={116} w={52} d={26} h={10} color="#e6dcc0" topColor="#efe7cf" stroke="#8a7f5a" />
            <circle
              className={state.di1 ? 'led-green-on' : 'led-off'}
              cx={iso(352, 129, 10).x}
              cy={iso(352, 129, 10).y}
              r="4.5"
            />
            <circle
              className={state.di2 ? 'led-green-on' : 'led-off'}
              cx={iso(378, 129, 10).x}
              cy={iso(378, 129, 10).y}
              r="4.5"
            />
            <text className="comp-label" x={iso(392, 116, 18).x + 10} y={iso(392, 116, 18).y}>DI 1 · DI 2</text>
            <text className="comp-sub" x={iso(392, 116, 4).x + 10} y={iso(392, 116, 4).y}>GPIO4 · GPIO5</text>
          </g>

          {/* ---------- relays + terminals ---------- */}
          {RELAYS.map((r) => {
            const on = state[r.key];
            const cx = r.u + RELAY_W / 2;
            const ledP = iso(r.u + 10, RELAY_V - 8, 2);
            const labelP = iso(cx - 12, 238);
            return (
              <g key={r.key}>
                <g
                  className="hit"
                  onMouseEnter={() => showTip(r.key)}
                  onMouseLeave={hideTip}
                  onClick={() => onToggleRelay(r.key)}
                >
                  <circle className={on ? 'led-on' : 'led-off'} cx={ledP.x} cy={ledP.y} r="4.5" />
                  <IsoBox
                    u={r.u}
                    v={RELAY_V}
                    w={RELAY_W}
                    d={RELAY_D}
                    h={RELAY_H}
                    color={on ? '#3d8ef0' : '#2f6fc4'}
                    topColor={on ? '#5ba4ff' : '#3f83da'}
                    stroke={on ? '#bfe0ff' : '#153a6e'}
                    strokeWidth={on ? 1.6 : 0.8}
                  />
                  {/* white spec sticker on top */}
                  <polygon
                    points={pts(
                      iso(r.u + 8, RELAY_V + 8, RELAY_H + 0.5),
                      iso(r.u + RELAY_W - 8, RELAY_V + 8, RELAY_H + 0.5),
                      iso(r.u + RELAY_W - 8, RELAY_V + RELAY_D - 8, RELAY_H + 0.5),
                      iso(r.u + 8, RELAY_V + RELAY_D - 8, RELAY_H + 0.5)
                    )}
                    fill="#e9edf2"
                    opacity="0.92"
                  />
                </g>
                {/* screw terminal in front */}
                <g style={{ pointerEvents: 'none' }}>
                  <IsoBox u={r.u + 6} v={206} w={50} d={20} h={14} color="#2fa05a" stroke="#0d3a1e" />
                  {[16, 28, 40].map((du, i) => {
                    const p = iso(r.u + 6 + du, 216, 14);
                    return <ellipse key={i} cx={p.x} cy={p.y} rx="3.4" ry="2" fill="#e8eef2" />;
                  })}
                </g>
                <text className="comp-label" x={labelP.x} y={labelP.y + 14}>
                  {r.key.replace('relay', 'RY')} · GPIO{r.gpio}
                </text>
              </g>
            );
          })}

          {/* header text */}
          <text className="comp-label" x="30" y="30">RelayBoardEspBuildIn · digital twin</text>
          <text className="comp-sub" x="30" y="46">hover a part · click a relay or the buzzer · packets = live signals</text>
        </svg>

        <div className="relay-iso-legend">
          <span><i className="relay-iso-dot pwr" /> Power rail</span>
          <span><i className="relay-iso-dot uart" /> UART / serial</span>
          <span><i className="relay-iso-dot gpio" /> GPIO out (relay / buzzer)</span>
          <span><i className="relay-iso-dot di" /> Digital input</span>
        </div>
      </div>
    </div>
  );
};

export default RelayBoardTwin3D;
