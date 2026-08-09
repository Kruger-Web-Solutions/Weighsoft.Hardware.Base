import { FC, useEffect, useState } from 'react';

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
    title: 'AC input · L/N',
    body: 'Mains feed (90–250V AC) into the onboard switching supply. Fuse and MOV sit right next to it.'
  },
  dc: {
    title: 'DC input · 7–30V / GND / 5V',
    body: 'Alternative DC feed into the LM2596 buck, or direct regulated 5V on the third screw.'
  },
  trafo: {
    title: 'Switching transformer',
    body: 'Isolation transformer of the AC-DC supply, with filter caps and fuse beside it.'
  },
  psu: {
    title: 'LM2596S-5.0 buck + 330 coil',
    body: 'Steps the input down to 5V for the relay coils. Orange packets = power rail flow.'
  },
  ams: {
    title: 'AMS1117 regulator',
    body: '5V → 3.3V linear regulator feeding the ESP-12F.'
  },
  esp: {
    title: 'ESP-12F (ESP8266MOD)',
    body: 'WiFi MCU with PCB antenna. Drives K1–K4 via the RY jumpers, reads DI on GPIO4/5.'
  },
  prog: {
    title: 'Programming header',
    body: '5V, GND, TXD0, RXD0 + IO row. Laptop → MAX3232 → here. Hold IO0 low at reset for flash mode.'
  },
  rst: {
    title: 'RST button',
    body: 'Hardware reset for the ESP-12F.'
  },
  max: {
    title: 'MAX3232 adapter',
    body: 'RS-232 ↔ TTL level shifter between the USB-serial cable and the ESP UART pins. Blue packets = serial.'
  },
  io: {
    title: 'IO breakout · DI 1 / DI 2',
    body: 'IO5, IO4, IO0, IO2, IO15 pins. DI 1 = GPIO4, DI 2 = GPIO5 (pullup, close to GND to trigger). GPIO5 also drives the blue LED.'
  },
  jump: {
    title: 'RY jumper caps',
    body: 'GPIO16/14/12/13 connect to the relay drivers through these caps — move a cap or use a wire to remap any relay.'
  },
  leds: {
    title: 'Indicator LEDs',
    body: 'Power LED plus one red LED per relay channel, following the coil drive.'
  },
  relay1: {
    title: 'K1 · Songle SRD-05VDC',
    body: 'GPIO16 → jumper → driver → coil, active HIGH. Click to toggle. Note: GPIO16 pulses briefly at power-up.'
  },
  relay2: {
    title: 'K2 · Songle SRD-05VDC',
    body: 'GPIO14 → jumper → driver → coil, active HIGH. COM / NO / NC on the terminal to the right.'
  },
  relay3: {
    title: 'K3 · Songle SRD-05VDC',
    body: 'GPIO12 → jumper → driver → coil, active HIGH. 10A dry contacts: AC 250V / DC 30V.'
  },
  relay4: {
    title: 'K4 · Songle SRD-05VDC',
    body: 'GPIO13 → jumper → driver → coil, active HIGH. Driver cluster with flyback diode sits to its left.'
  }
};

/* ---------- isometric projection ---------- */

const K = 0.95;
const K2 = 0.5;
const OX = 500;
const OY = 105;

const iso = (u: number, v: number, z = 0) => ({ x: OX + (u - v) * K, y: OY + (u + v) * K2 - z });

const pts = (...p: { x: number; y: number }[]) => p.map((q) => `${q.x.toFixed(1)},${q.y.toFixed(1)}`).join(' ');

const shade = (hex: string, f: number) => {
  const n = parseInt(hex.slice(1), 16);
  const r = Math.min(255, Math.round(((n >> 16) & 255) * f));
  const g = Math.min(255, Math.round(((n >> 8) & 255) * f));
  const b = Math.min(255, Math.round((n & 255) * f));
  return `rgb(${r},${g},${b})`;
};

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

/* ---------- layout: board 300 x 280, mirrors the real PCB photo ---------- */

const RELAYS: { key: RelayKey; gpio: number; silk: string; v: number }[] = [
  { key: 'relay1', gpio: 16, silk: 'K1', v: 12 },
  { key: 'relay2', gpio: 14, silk: 'K2', v: 78 },
  { key: 'relay3', gpio: 12, silk: 'K3', v: 144 },
  { key: 'relay4', gpio: 13, silk: 'K4', v: 210 }
];

const RELAY_U = 190;
const RELAY_W = 78;
const RELAY_D = 60;
const RELAY_H = 32;

/* relay signal path: ESP → RY jumper → driver column → relay */
const relayPipe = (i: number): string => {
  const vc = RELAYS[i].v + RELAY_D / 2;
  const capU = 108 + i * 14;
  const corridorU = 170 + i * 4;
  return boardPath(
    [
      [85, 95 + i * 6],
      [capU, 162],
      [corridorU, 162],
      [corridorU, vc],
      [RELAY_U - 2, vc]
    ],
    3
  );
};

const POWER_PIPE = boardPath(
  [
    [12, 250],
    [70, 250],
    [70, 206],
    [95, 186],
    [148, 46],
    [88, 88]
  ],
  4
);

const UART_PIPE = boardPath(
  [
    [-52, -14],
    [95, 20],
    [70, 55]
  ],
  8
);

const DI1_PIPE = boardPath(
  [
    [47, 141],
    [47, 131],
    [60, 124]
  ],
  3
);

const DI2_PIPE = boardPath(
  [
    [35, 141],
    [35, 129],
    [50, 122]
  ],
  3
);

const RelayBoardTwin3D: FC<RelayBoardTwin3DProps> = ({
  state,
  powerOn = true,
  uartLive = false,
  onToggleRelay
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

  const bA = iso(0, 0);
  const bB = iso(300, 0);
  const bC = iso(300, 280);
  const bD = iso(0, 280);
  const thickness = 20;

  const relayStates = [state.relay1, state.relay2, state.relay3, state.relay4];

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
          viewBox="0 0 1000 470"
          role="img"
          aria-label="Isometric digital twin of the ESP12F_Relay_X4 board"
        >
          <defs>
            <linearGradient id="pcbTop" x1="0" y1="0" x2="1" y2="1">
              <stop offset="0%" stopColor="#1c8a4e" />
              <stop offset="50%" stopColor="#137243" />
              <stop offset="100%" stopColor="#0c5833" />
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

          <rect x="0" y="0" width="1000" height="470" fill="url(#stageGlow)" />

          {/* ---------- PCB slab ---------- */}
          <g filter="url(#boardShadow)">
            <polygon
              points={pts({ x: bD.x, y: bD.y + thickness }, { x: bC.x, y: bC.y + thickness }, bC, bD)}
              fill="#05301a"
            />
            <polygon
              points={pts({ x: bC.x, y: bC.y + thickness }, { x: bB.x, y: bB.y + thickness }, bB, bC)}
              fill="#083f22"
            />
            <polygon points={pts(bA, bB, bC, bD)} fill="url(#pcbTop)" stroke="#7fe0a4" strokeOpacity="0.28" />
          </g>

          {/* silkscreen grid */}
          <g opacity="0.08" stroke="#d8ffe6" strokeWidth="1" style={{ pointerEvents: 'none' }}>
            {Array.from({ length: 11 }).map((_, i) => {
              const a = iso(14 + i * 27, 10);
              const b = iso(14 + i * 27, 270);
              return <line key={`v${i}`} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
            })}
            {Array.from({ length: 10 }).map((_, i) => {
              const a = iso(10, 16 + i * 27);
              const b = iso(290, 16 + i * 27);
              return <line key={`h${i}`} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
            })}
          </g>

          {/* silk name + logo */}
          <text
            transform={`matrix(${K},${K2},${-K},${K2},${iso(30, 274).x},${iso(30, 274).y})`}
            fill="#eafff2"
            opacity="0.3"
            fontFamily="IBM Plex Mono, Consolas, monospace"
            fontSize="12"
            style={{ pointerEvents: 'none' }}
          >
            WEIGHSOFT · ESP12F_RELAY_X4 · 2026-04-01
          </text>
          <polygon
            points={pts(iso(238, 250), iso(252, 244), iso(266, 250), iso(252, 262))}
            fill="#eafff2"
            opacity="0.22"
            style={{ pointerEvents: 'none' }}
          />

          {/* mounting holes */}
          {[iso(14, 14), iso(286, 14), iso(286, 266), iso(14, 266), iso(14, 140)].map((p, i) => (
            <g key={i} style={{ pointerEvents: 'none' }}>
              <ellipse cx={p.x} cy={p.y} rx="8" ry="4.6" fill="#caa64e" />
              <ellipse cx={p.x} cy={p.y} rx="4.2" ry="2.4" fill="#081720" />
            </g>
          ))}

          {/* ---------- signal pipes ---------- */}
          <Pipe kind="pwr" active={!!powerOn} duration="2.8s" packets={4} d={POWER_PIPE} />
          <Pipe kind="uart" active={uartBurst || uartLive} duration="1.8s" packets={4} d={UART_PIPE} />
          <Pipe kind="di" active={state.di1} duration="1.2s" packets={2} d={DI1_PIPE} />
          <Pipe kind="di" active={state.di2} duration="1.2s" packets={2} d={DI2_PIPE} />
          {RELAYS.map((r, i) => (
            <Pipe key={r.key} kind="gpio" active={state[r.key]} duration="1.6s" packets={3} d={relayPipe(i)} />
          ))}

          {/* ---------- MAX3232 (off-board, top-left) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('max')} onMouseLeave={hideTip}>
            <IsoBox u={-100} v={-40} w={52} d={38} h={8} color="#17603a" topColor="#1e7546" stroke="#8fd9a8" />
            <IsoBox u={-86} v={-30} w={24} d={17} h={6} color="#14181e" />
            <text className="comp-label" x={iso(-100, -2).x - 60} y={iso(-100, -2).y + 26}>MAX3232</text>
            <text className="comp-sub" x={iso(-100, -2).x - 60} y={iso(-100, -2).y + 39}>USB-serial bridge</text>
          </g>

          {/* ---------- programming header (top-left) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('prog')} onMouseLeave={hideTip}>
            <IsoBox u={56} v={10} w={78} d={26} h={2} color="#0d3b23" topColor="#0f4429" />
            {Array.from({ length: 8 }).map((_, i) => {
              const p1 = iso(62 + i * 9, 16, 2);
              const p2 = iso(62 + i * 9, 28, 2);
              return (
                <g key={i}>
                  <circle cx={p1.x} cy={p1.y} r="2.4" fill="#d8b44a" />
                  <circle cx={p2.x} cy={p2.y} r="2.4" fill="#d8b44a" />
                </g>
              );
            })}
            <text className="comp-label" x={iso(56, 10, 22).x} y={iso(56, 10, 22).y}>PROG</text>
            <text className="comp-sub" x={iso(56, 10, 10).x} y={iso(56, 10, 10).y}>5V GND TX RX IO0</text>
          </g>

          {/* ---------- RST button ---------- */}
          <g className="hit" onMouseEnter={() => showTip('rst')} onMouseLeave={hideTip}>
            <IsoBox u={158} v={14} w={16} d={14} h={6} color="#9aa4ae" topColor="#c3ccd6" />
            <Cyl u={166} v={21} r={4.5} h={9} color="#30343a" topColor="#42474e" />
            <text className="comp-sub" x={iso(176, 14, 14).x} y={iso(176, 14, 14).y}>RST</text>
          </g>

          {/* ---------- ESP-12F with antenna (left) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('esp')} onMouseLeave={hideTip}>
            <IsoBox u={8} v={55} w={80} d={70} h={4} color="#10222e" topColor="#132a3a" />
            {/* PCB antenna zigzag */}
            <path
              d={boardPath(
                [
                  [13, 62],
                  [26, 62],
                  [26, 72],
                  [13, 72],
                  [13, 82],
                  [26, 82],
                  [26, 92],
                  [13, 92],
                  [13, 102],
                  [26, 102],
                  [26, 112],
                  [13, 112]
                ],
                5
              )}
              stroke="#d8b44a"
              strokeWidth="1.6"
              fill="none"
            />
            {/* RF shield */}
            <IsoBox
              u={32}
              v={58}
              w={52}
              d={64}
              h={13}
              color="#93a0ae"
              topColor="#c3ccd6"
              stroke="#e8eef4"
              strokeWidth={0.6}
            />
            {/* castellated pads on right edge */}
            {Array.from({ length: 7 }).map((_, i) => {
              const p = iso(88, 60 + i * 9, 2);
              return <rect key={i} x={p.x - 2} y={p.y - 2} width="4.5" height="4.5" fill="#d8b44a" />;
            })}
            <text className="comp-label" x={iso(2, 55, 40).x - 10} y={iso(2, 55, 40).y}>ESP-12F</text>
            <text className="comp-sub" x={iso(2, 55, 28).x - 10} y={iso(2, 55, 28).y}>ESP8266MOD · WiFi</text>
          </g>

          {/* ---------- AMS1117 ---------- */}
          <g className="hit" onMouseEnter={() => showTip('ams')} onMouseLeave={hideTip}>
            <IsoBox u={142} v={38} w={18} d={13} h={5} color="#20242c" topColor="#14181e" />
            <text className="comp-sub" x={iso(138, 34, 26).x} y={iso(138, 34, 26).y}>AMS1117</text>
          </g>

          {/* ---------- indicator LED column (left edge) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('leds')} onMouseLeave={hideTip}>
            <circle className={powerOn ? 'led-on' : 'led-off'} cx={iso(10, 152, 3).x} cy={iso(10, 152, 3).y} r="4" />
            {relayStates.map((on, i) => {
              const p = iso(10, 161 + i * 9, 3);
              return <circle key={i} className={on ? 'led-on' : 'led-off'} cx={p.x} cy={p.y} r="3.4" />;
            })}
          </g>

          {/* ---------- IO breakout row (DI pins) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('io')} onMouseLeave={hideTip}>
            <IsoBox u={28} v={136} w={64} d={12} h={2} color="#0d3b23" topColor="#0f4429" />
            {[35, 47, 59, 71, 83].map((u, i) => {
              const p = iso(u, 142, 2);
              return <circle key={i} cx={p.x} cy={p.y} r="2.4" fill="#d8b44a" />;
            })}
            <circle
              className={state.di2 ? 'led-green-on' : 'led-off'}
              cx={iso(35, 133, 4).x}
              cy={iso(35, 133, 4).y}
              r="3.4"
            />
            <circle
              className={state.di1 ? 'led-green-on' : 'led-off'}
              cx={iso(47, 133, 4).x}
              cy={iso(47, 133, 4).y}
              r="3.4"
            />
          </g>

          {/* ---------- IO16-13 row + RY jumper caps ---------- */}
          <g className="hit" onMouseEnter={() => showTip('jump')} onMouseLeave={hideTip}>
            <IsoBox u={100} v={156} w={66} d={14} h={2} color="#0d3b23" topColor="#0f4429" />
            {[108, 122, 136, 150].map((u, i) => (
              <IsoBox key={i} u={u - 4} v={158} w={9} d={10} h={6} color="#1a1a22" topColor="#26262f" />
            ))}
            <text className="comp-sub" x={iso(100, 172).x - 30} y={iso(100, 172).y + 20}>RY1-RY4 jumpers</text>
          </g>

          {/* ---------- DC terminal (left edge) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('dc')} onMouseLeave={hideTip}>
            <IsoBox u={2} v={175} w={22} d={42} h={16} color="#2fa05a" stroke="#0d3a1e" />
            {[183, 196, 209].map((v, i) => {
              const p = iso(13, v, 16);
              return <ellipse key={i} cx={p.x} cy={p.y} rx="3.6" ry="2.1" fill="#e8eef2" />;
            })}
          </g>

          {/* ---------- LM2596 + coil + caps ---------- */}
          <g className="hit" onMouseEnter={() => showTip('psu')} onMouseLeave={hideTip}>
            <IsoBox u={32} v={192} w={38} d={26} h={6} color="#20242c" topColor="#14181e" />
            {Array.from({ length: 5 }).map((_, i) => {
              const p = iso(72, 195 + i * 5, 2);
              return <rect key={i} x={p.x - 1.5} y={p.y - 1.5} width="3.5" height="3.5" fill="#c9c9c9" />;
            })}
            <Cyl u={95} v={185} r={13} h={12} color="#20242c" topColor="#2e3642" />
            <ellipse cx={iso(95, 185, 12).x} cy={iso(95, 185, 12).y} rx="5" ry="2.8" fill="#14181e" />
          </g>
          <Cyl u={120} v={150} r={10} h={20} color="#26303c" topColor="#3c4c5e" />
          <Cyl u={112} v={226} r={10} h={18} color="#26303c" topColor="#3c4c5e" />

          {/* ---------- AC terminal + fuse + discs (bottom-left) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('ac')} onMouseLeave={hideTip}>
            <IsoBox u={2} v={235} w={20} d={34} h={16} color="#2fa05a" stroke="#0d3a1e" />
            {[243, 259].map((v, i) => {
              const p = iso(12, v, 16);
              return <ellipse key={i} cx={p.x} cy={p.y} rx="3.6" ry="2.1" fill="#e8eef2" />;
            })}
          </g>
          <Cyl u={32} v={248} r={6} h={9} color="#14181e" topColor="#22262e" />
          <Cyl u={54} v={252} r={5.5} h={8} color="#2255c4" topColor="#3a6fe0" />
          <Cyl u={152} v={247} r={5.5} h={8} color="#2255c4" topColor="#3a6fe0" />

          {/* ---------- transformer (bottom-center) ---------- */}
          <g className="hit" onMouseEnter={() => showTip('trafo')} onMouseLeave={hideTip}>
            <IsoBox u={88} v={246} w={42} d={32} h={20} color="#d9b13b" topColor="#e6c355" stroke="#7a5f10" />
            <IsoBox u={96} v={252} w={26} d={20} h={22} color="#8a6d14" />
            <text className="comp-sub" x={iso(88, 278).x - 26} y={iso(88, 278).y + 16}>transformer</text>
          </g>
          <IsoBox u={148} v={258} w={16} d={11} h={5} color="#20242c" topColor="#14181e" />
          <IsoBox u={60} v={232} w={14} d={10} h={5} color="#20242c" topColor="#14181e" />

          {/* ---------- relay driver clusters ---------- */}
          <g style={{ pointerEvents: 'none' }}>
            {RELAYS.map((r, i) => {
              const vc = r.v + RELAY_D / 2;
              return (
                <g key={i}>
                  <IsoBox u={166} v={vc - 12} w={9} d={5} h={2.5} color="#8a6d3a" />
                  <IsoBox u={166} v={vc - 4} w={9} d={5} h={2.5} color="#2a2a2a" />
                  <IsoBox u={166} v={vc + 4} w={9} d={5} h={2.5} color="#8a6d3a" />
                  <circle cx={iso(180, vc - 8, 2).x} cy={iso(180, vc - 8, 2).y} r="2.2" fill="#b03030" />
                </g>
              );
            })}
          </g>

          {/* ---------- relays K1-K4 + terminals (right side) ---------- */}
          {RELAYS.map((r) => {
            const on = state[r.key];
            const vc = r.v + RELAY_D / 2;
            const label = iso(300, vc + 10);
            return (
              <g key={r.key}>
                <g
                  className="hit"
                  onMouseEnter={() => showTip(r.key)}
                  onMouseLeave={hideTip}
                  onClick={() => onToggleRelay(r.key)}
                >
                  <IsoBox
                    u={RELAY_U}
                    v={r.v}
                    w={RELAY_W}
                    d={RELAY_D}
                    h={RELAY_H}
                    color={on ? '#3d8ef0' : '#2f6fc4'}
                    topColor={on ? '#5ba4ff' : '#3f83da'}
                    stroke={on ? '#bfe0ff' : '#153a6e'}
                    strokeWidth={on ? 1.6 : 0.8}
                  />
                  <polygon
                    points={pts(
                      iso(RELAY_U + 8, r.v + 8, RELAY_H + 0.5),
                      iso(RELAY_U + RELAY_W - 8, r.v + 8, RELAY_H + 0.5),
                      iso(RELAY_U + RELAY_W - 8, r.v + RELAY_D - 8, RELAY_H + 0.5),
                      iso(RELAY_U + 8, r.v + RELAY_D - 8, RELAY_H + 0.5)
                    )}
                    fill="#e9edf2"
                    opacity="0.92"
                  />
                  <text
                    transform={`matrix(${K},${K2},${-K},${K2},${iso(RELAY_U + 14, r.v + 22, RELAY_H + 0.5).x},${
                      iso(RELAY_U + 14, r.v + 22, RELAY_H + 0.5).y
                    })`}
                    fill="#2b3d55"
                    fontFamily="IBM Plex Mono, Consolas, monospace"
                    fontSize="8.5"
                    fontWeight="700"
                    style={{ pointerEvents: 'none' }}
                  >
                    SONGLE
                  </text>
                  <text
                    transform={`matrix(${K},${K2},${-K},${K2},${iso(RELAY_U + 12, r.v + 36, RELAY_H + 0.5).x},${
                      iso(RELAY_U + 12, r.v + 36, RELAY_H + 0.5).y
                    })`}
                    fill="#54677f"
                    fontFamily="IBM Plex Mono, Consolas, monospace"
                    fontSize="6"
                    style={{ pointerEvents: 'none' }}
                  >
                    SRD-05VDC-SL-C
                  </text>
                </g>
                <g style={{ pointerEvents: 'none' }}>
                  <IsoBox u={272} v={r.v + 2} w={26} d={56} h={15} color="#2fa05a" stroke="#0d3a1e" />
                  {[10, 28, 46].map((dv, i) => {
                    const p = iso(285, r.v + 2 + dv, 15);
                    return <ellipse key={i} cx={p.x} cy={p.y} rx="3.4" ry="2" fill="#e8eef2" />;
                  })}
                </g>
                <text className="comp-label" x={label.x + 10} y={label.y - 6}>
                  {r.silk} · GPIO{r.gpio}
                </text>
                <text className="comp-sub" x={label.x + 10} y={label.y + 7}>COM · NO · NC</text>
              </g>
            );
          })}

          {/* left-side callouts with leader lines */}
          <g style={{ pointerEvents: 'none' }}>
            <g stroke="#8fb0c8" strokeOpacity="0.45" strokeWidth="1">
              <line x1="268" y1="192" x2="398" y2="188" />
              <line x1="215" y1="236" x2="310" y2="200" />
              <line x1="242" y1="279" x2="260" y2="230" />
            </g>
            <text className="comp-label" x="108" y="195">DI 1 · DI 2 · GPIO4/5</text>
            <text className="comp-label" x="108" y="239">DC IN · 7-30V / 5V</text>
            <text className="comp-label" x="108" y="282">AC IN · L/N</text>
          </g>

          {/* header text */}
          <text className="comp-label" x="26" y="26">ESP12F_Relay_X4 · digital twin</text>
          <text className="comp-sub" x="26" y="42">hover a part · click a relay · packets = live signals</text>
        </svg>

        <div className="relay-iso-legend">
          <span><i className="relay-iso-dot pwr" /> Power rail</span>
          <span><i className="relay-iso-dot uart" /> UART / serial</span>
          <span><i className="relay-iso-dot gpio" /> GPIO out (relay)</span>
          <span><i className="relay-iso-dot di" /> Digital input</span>
        </div>
      </div>
    </div>
  );
};

export default RelayBoardTwin3D;
