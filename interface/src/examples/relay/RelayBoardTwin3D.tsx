import { FC, useEffect, useMemo, useState } from 'react';

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
    body: 'Mains feed into the onboard supply (transformer / rectifier). Powers the whole board.'
  },
  dc: {
    title: 'DC 7–30V input',
    body: 'Alternative DC feed into the LM2596 buck converter.'
  },
  psu: {
    title: 'Power section',
    body: 'LM2596 buck → 5V for relay coils; AMS1117 → 3.3V for the ESP-12F. Orange packets = power rail flow.'
  },
  trafo: {
    title: 'Transformer + caps',
    body: 'Isolation transformer with bulk electrolytic capacitors smoothing the rails.'
  },
  esp: {
    title: 'ESP-12F (DOIT)',
    body: 'WiFi MCU. Runs firmware, drives RY1–RY4, buzzer on GPIO15, reads DI 1/2, UART TX0/RX0 for flash/debug.'
  },
  uart: {
    title: 'UART / flash header',
    body: 'TX0, RX0, GND, 3V3, IO0. Blue packets = serial traffic. IO0 low at reset = flash mode.'
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
    body: 'INPUT_PULLUP: close the pin to GND to trigger. Lights up here live when the input is active.'
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
    body: 'GPIO12 → driver → coil. Active LOW drive on this board family.'
  },
  relay4: {
    title: 'RY4 · Songle SRD-05VDC',
    body: 'GPIO13 → driver → coil. 10A class contacts for switched loads.'
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
  kind?: 'gpio' | 'uart' | 'pwr' | 'di';
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
            <animateMotion dur={duration} begin={`${i * 0.35}s`} repeatCount="indefinite" path={d} />
          )}
        </circle>
      ))}
    </g>
  );
};

const Capacitor: FC<{ x: number; y: number; r?: number }> = ({ x, y, r = 9 }) => (
  <g style={{ pointerEvents: 'none' }}>
    <ellipse cx={x} cy={y} rx={r} ry={r * 0.75} fill="#1a1a24" stroke="#555" />
    <ellipse cx={x} cy={y - 3} rx={r * 0.8} ry={r * 0.55} fill="#2e3a4a" stroke="#7a8a9a" strokeWidth="0.6" />
    <line x1={x - r * 0.5} y1={y - 3} x2={x + r * 0.5} y2={y - 3} stroke="#9ab" strokeWidth="0.8" />
  </g>
);

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
      window.setTimeout(() => setUartBurst(false), 900);
    }, 2400);
    return () => window.clearInterval(id);
  }, [uartLive]);

  const pEsp = useMemo(() => iso(210, 40, 28), []);
  const pAc = useMemo(() => iso(20, 20, 12), []);
  const pDc = useMemo(() => iso(70, -20, 12), []);
  const pPsu = useMemo(() => iso(70, 90, 18), []);
  const pTrafo = useMemo(() => iso(30, 150, 16), []);
  const pUart = useMemo(() => iso(280, 20, 20), []);
  const pMax = useMemo(() => iso(340, -40, 16), []);
  const pLed = useMemo(() => iso(150, 10, 10), []);
  const pBuzz = useMemo(() => iso(255, 95, 16), []);
  const pDi = useMemo(() => iso(300, 130, 12), []);

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
            <radialGradient id="copperPad" cx="0.5" cy="0.5" r="0.5">
              <stop offset="0%" stopColor="#f0c674" />
              <stop offset="100%" stopColor="#9a7020" />
            </radialGradient>
            <filter id="softShadow" x="-20%" y="-20%" width="140%" height="140%">
              <feDropShadow dx="0" dy="10" stdDeviation="8" floodOpacity="0.45" />
            </filter>
          </defs>

          {/* PCB slab */}
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

          {/* Mounting holes */}
          {[iso(8, 8, 0), iso(272, -18, 0), iso(300, 195, 0), iso(30, 215, 0)].map((p, i) => (
            <g key={i} style={{ pointerEvents: 'none' }}>
              <circle cx={p.x} cy={p.y} r="8" fill="url(#copperPad)" />
              <circle cx={p.x} cy={p.y} r="4" fill="#0d1117" />
            </g>
          ))}

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

          {/* DI pipes: DI header → ESP */}
          <Pipe
            kind="di"
            active={state.di1}
            duration="1.1s"
            packets={3}
            d={`M ${pDi.x - 8} ${pDi.y} C ${pDi.x - 40} ${pDi.y - 30}, ${pEsp.x + 40} ${pEsp.y + 60}, ${pEsp.x + 20} ${pEsp.y + 42}`}
          />
          <Pipe
            kind="di"
            active={state.di2}
            duration="1.1s"
            packets={3}
            d={`M ${pDi.x + 12} ${pDi.y + 10} C ${pDi.x - 30} ${pDi.y - 10}, ${pEsp.x + 55} ${pEsp.y + 70}, ${pEsp.x + 32} ${pEsp.y + 44}`}
          />

          {/* Buzzer pipe: ESP → buzzer */}
          {hasBuzzer && (
            <Pipe
              kind="gpio"
              active={state.buzzer}
              duration="0.9s"
              packets={2}
              d={
                `M ${pEsp.x - 5} ${pEsp.y + 38} ` +
                `C ${pEsp.x - 15} ${pEsp.y + 60}, ${pBuzz.x + 20} ${pBuzz.y - 40}, ${pBuzz.x + 2} ${pBuzz.y - 14}`
              }
            />
          )}

          {/* GPIO pipes: ESP → each relay */}
          {RELAYS.map((r) => {
            const ry = iso(r.x / 3.2, 175, 8);
            const d =
              `M ${pEsp.x + 8} ${pEsp.y + 40} ` +
              `C ${pEsp.x - 20} ${pEsp.y + 90}, ${ry.x} ${ry.y - 50}, ${ry.x} ${ry.y - 8}`;
            return <Pipe key={r.key} kind="gpio" active={state[r.key]} duration="1.25s" packets={3} d={d} />;
          })}

          {/* AC terminal */}
          <g className="hit" onMouseEnter={() => showTip('ac')} onMouseLeave={() => setTip(null)}>
            <rect x={pAc.x - 28} y={pAc.y - 14} width="56" height="28" rx="3" fill="#2f8a4e" stroke="#0d3a1e" />
            <circle cx={pAc.x - 12} cy={pAc.y} r="4" fill="#ddd" />
            <circle cx={pAc.x + 12} cy={pAc.y} r="4" fill="#ddd" />
            <text className="comp-label" x={pAc.x - 22} y={pAc.y - 22}>AC IN</text>
            <text className="comp-sub" x={pAc.x - 26} y={pAc.y + 36}>L / N mains</text>
          </g>

          {/* DC terminal */}
          <g className="hit" onMouseEnter={() => showTip('dc')} onMouseLeave={() => setTip(null)}>
            <rect x={pDc.x - 32} y={pDc.y - 12} width="64" height="24" rx="3" fill="#2f8a4e" stroke="#0d3a1e" />
            <circle cx={pDc.x - 16} cy={pDc.y} r="3.5" fill="#ddd" />
            <circle cx={pDc.x} cy={pDc.y} r="3.5" fill="#ddd" />
            <circle cx={pDc.x + 16} cy={pDc.y} r="3.5" fill="#ddd" />
            <text className="comp-label" x={pDc.x - 30} y={pDc.y - 20}>DC 7-30V</text>
          </g>

          {/* Transformer + caps */}
          <g className="hit" onMouseEnter={() => showTip('trafo')} onMouseLeave={() => setTip(null)}>
            <rect x={pTrafo.x - 26} y={pTrafo.y - 22} width="52" height="44" rx="4" fill="#c9a227" stroke="#7a5f10" />
            <rect x={pTrafo.x - 18} y={pTrafo.y - 14} width="36" height="28" rx="2" fill="#8a6d14" />
            <text className="comp-sub" x={pTrafo.x - 24} y={pTrafo.y + 38}>transformer</text>
          </g>
          <Capacitor x={pTrafo.x + 46} y={pTrafo.y - 8} />
          <Capacitor x={pTrafo.x + 66} y={pTrafo.y + 8} r={7} />
          <Capacitor x={pPsu.x + 52} y={pPsu.y + 16} r={8} />

          {/* Inductor */}
          <g style={{ pointerEvents: 'none' }}>
            <circle cx={pPsu.x + 30} cy={pPsu.y - 22} r="11" fill="#2a2a2a" stroke="#666" />
            <circle cx={pPsu.x + 30} cy={pPsu.y - 22} r="5" fill="#444" />
            <text className="comp-sub" x={pPsu.x + 16} y={pPsu.y - 38}>330 coil</text>
          </g>

          {/* PSU block */}
          <g className="hit" onMouseEnter={() => showTip('psu')} onMouseLeave={() => setTip(null)}>
            <rect x={pPsu.x - 40} y={pPsu.y - 28} width="90" height="56" rx="6" fill="#2a2a2a" stroke="#666" />
            <rect x={pPsu.x - 28} y={pPsu.y - 18} width="28" height="36" rx="2" fill="#c9a227" />
            <circle cx={pPsu.x + 22} cy={pPsu.y} r="12" fill="#111" stroke="#444" />
            <text className="comp-label" x={pPsu.x - 36} y={pPsu.y - 38}>PSU</text>
            <text className="comp-sub" x={pPsu.x - 40} y={pPsu.y + 46}>LM2596 · AMS1117</text>
          </g>

          {/* Power LED */}
          <g className="hit" onMouseEnter={() => showTip('pwrled')} onMouseLeave={() => setTip(null)}>
            <circle className={powerOn ? 'led-on' : 'led-off'} cx={pLed.x} cy={pLed.y} r="7" />
            <text className="comp-label" x={pLed.x + 12} y={pLed.y + 4}>PWR LED</text>
          </g>

          {/* ESP-12F */}
          <g className="hit" onMouseEnter={() => showTip('esp')} onMouseLeave={() => setTip(null)}>
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
            {Array.from({ length: 8 }).map((_, i) => (
              <rect key={i} x={pEsp.x - 46 + i * 13} y={pEsp.y + 46} width="6" height="8" fill="#c9a227" />
            ))}
            <text className="comp-label" x={pEsp.x - 40} y={pEsp.y - 44}>ESP-12F</text>
            <text className="comp-sub" x={pEsp.x - 40} y={pEsp.y + 66}>WiFi MCU · GPIO hub</text>
          </g>

          {/* UART header */}
          <g className="hit" onMouseEnter={() => showTip('uart')} onMouseLeave={() => setTip(null)}>
            <rect x={pUart.x - 14} y={pUart.y - 8} width="28" height="70" rx="3" fill="#e6dcc0" stroke="#8a7f5a" />
            {[0, 1, 2, 3, 4].map((i) => (
              <circle key={i} cx={pUart.x} cy={pUart.y + 6 + i * 12} r="3" fill="#333" />
            ))}
            <text className="comp-label" x={pUart.x + 20} y={pUart.y + 8}>UART</text>
            <text className="comp-sub" x={pUart.x + 20} y={pUart.y + 22}>TX RX IO0</text>
          </g>

          {/* MAX3232 off-board */}
          <g className="hit" onMouseEnter={() => showTip('uart')} onMouseLeave={() => setTip(null)}>
            <rect x={pMax.x - 36} y={pMax.y - 22} width="72" height="44" rx="5" fill="#1f6b3a" stroke="#8fd9a8" />
            <rect x={pMax.x - 16} y={pMax.y - 12} width="32" height="24" rx="2" fill="#111" />
            <text className="comp-label" x={pMax.x - 34} y={pMax.y - 30}>MAX3232</text>
            <text className="comp-sub" x={pMax.x - 40} y={pMax.y + 38}>USB-Serial bridge</text>
          </g>

          {/* Buzzer */}
          {hasBuzzer && (
            <g
              className="hit"
              onMouseEnter={() => showTip('buzzer')}
              onMouseLeave={() => setTip(null)}
              onClick={() => onToggleBuzzer && onToggleBuzzer()}
            >
              {state.buzzer && (
                <>
                  <circle className="buzz-ring" cx={pBuzz.x} cy={pBuzz.y} r="16" />
                  <circle className="buzz-ring buzz-ring-2" cx={pBuzz.x} cy={pBuzz.y} r="22" />
                </>
              )}
              <circle cx={pBuzz.x} cy={pBuzz.y} r="14" fill="#0d0d12" stroke="#3a3a44" strokeWidth="2" />
              <circle cx={pBuzz.x} cy={pBuzz.y} r="4" fill={state.buzzer ? '#ffd54a' : '#2a2a34'} />
              <text className="comp-label" x={pBuzz.x - 24} y={pBuzz.y + 32}>BUZZER</text>
              <text className="comp-sub" x={pBuzz.x - 24} y={pBuzz.y + 44}>GPIO15</text>
            </g>
          )}

          {/* DI header */}
          <g className="hit" onMouseEnter={() => showTip('di')} onMouseLeave={() => setTip(null)}>
            <rect x={pDi.x - 20} y={pDi.y - 12} width="44" height="26" rx="3" fill="#e6dcc0" stroke="#8a7f5a" />
            <circle className={state.di1 ? 'led-green-on' : 'led-off'} cx={pDi.x - 8} cy={pDi.y} r="5" />
            <circle className={state.di2 ? 'led-green-on' : 'led-off'} cx={pDi.x + 12} cy={pDi.y} r="5" />
            <text className="comp-label" x={pDi.x - 18} y={pDi.y + 32}>DI 1 · DI 2</text>
            <text className="comp-sub" x={pDi.x - 18} y={pDi.y + 44}>GPIO4 · GPIO5</text>
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
                  <circle className={on ? 'led-on' : 'led-off'} cx={p.x} cy={p.y - 22} r="5" />
                  <rect x={p.x - 14} y={p.y - 6} width="28" height="22" rx="2" fill="#0d2a55" />
                  <text className="comp-label" x={p.x - 18} y={p.y + 50}>
                    {r.key.replace('relay', 'RY')}
                  </text>
                  <text className="comp-sub" x={p.x - 26} y={p.y + 62}>
                    GPIO{r.gpio}
                  </text>
                </g>
                <g style={{ pointerEvents: 'none' }}>
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
            Hover a part · click relays or buzzer · packets = live signal in the pipe
          </text>
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
