import { FC } from 'react';

import { RelayBoardState } from './types';
import './relayTwin.css';

interface RelayBoardTwin3DProps {
  state: RelayBoardState;
  powerOn?: boolean;
  onToggleRelay: (key: keyof Pick<RelayBoardState, 'relay1' | 'relay2' | 'relay3' | 'relay4'>) => void;
}

const RelayBoardTwin3D: FC<RelayBoardTwin3DProps> = ({ state, powerOn = true, onToggleRelay }) => {
  return (
    <div className="relay-twin-stage">
      <div className="relay-twin-board" aria-label="3D digital twin of ESP-12F relay board">
        <div className="relay-twin-silk" />
        <div className="relay-terminal" />
        <span className="relay-twin-label" style={{ top: 18, left: 40 }}>AC / DC IN</span>
        <div className={`relay-power-led ${powerOn ? 'on' : ''}`} title="Power LED (hardwired)" />
        <span className="relay-twin-label" style={{ top: 34, left: 230 }}>PWR</span>
        <div className="relay-psu" title="LM2596 / AMS1117 power section" />
        <span className="relay-twin-label" style={{ top: 168, left: 56 }}>PSU</span>
        <div className="relay-esp" title="DOIT ESP-12F" />
        <div className="relay-header" title="TX0 RX0 IO0 GND 3V3 header" />
        <span className="relay-twin-label" style={{ top: 200, right: 18 }}>UART</span>

        {([
          ['relay1', 'relay-r1', 'RY1 / GPIO16'],
          ['relay2', 'relay-r2', 'RY2 / GPIO14'],
          ['relay3', 'relay-r3', 'RY3 / GPIO12'],
          ['relay4', 'relay-r4', 'RY4 / GPIO13']
        ] as const).map(([key, cls, label]) => (
          <button
            key={key}
            type="button"
            className={`relay-module ${cls} ${state[key] ? 'on' : ''}`}
            title={`${label} — click to toggle`}
            onClick={() => onToggleRelay(key)}
          >
            <span className="coil-led" />
            <span className="tag">{label}</span>
          </button>
        ))}
      </div>
    </div>
  );
};

export default RelayBoardTwin3D;
