import { FC } from 'react';

import { Typography } from '@mui/material';

import { MessageBox, SectionContent } from '../../components';

import './relayTwin.css';

const WIRING = `SERIAL / FLASH (from your photos)
Laptop USB
  → Prolific USB-to-Serial (Windows: often COM2 if Bluetooth holds COM3)
  → DB9 cable
  → MAX3232 / MAX232 board
  → Relay board header:
       3V3  ← adapter +
       GND  ← adapter GND
       TX0 / RX0  ← crossed as needed with adapter TX/RX
       IO0 → GND during flash, then RST

POWER
  AC L/N  → onboard supply  OR  DC 7-30V → LM2596 → 5V
  5V → relay coils
  AMS1117 → 3.3V → ESP-12F

RELAYS (default firmware map — verify with click test)
  RY1 ← GPIO16   RY2 ← GPIO14   RY3 ← GPIO12   RY4 ← GPIO13
  Active LOW (LOW = coil ON)
  Screw terminals: COM / NO / NC per channel

SENSORS
  No onboard temperature sensor on this PCB.
  Breakout DI available on IO4 / IO5 (and others — avoid boot straps for outputs).`;

const RelayBoardWiring: FC = () => (
  <SectionContent title="Wiring diagram" titleGutter>
    <MessageBox
      level="info"
      message="Derived from your board photos: ESP-12F, 4× Songle relays, MAX3232 serial path, AC/DC power section."
      my={2}
    />
    <div className="wiring-box">{WIRING}</div>
    <Typography variant="body2" sx={{ mt: 2 }} color="textSecondary">
      Full mermaid diagrams: docs/RELAY-BOARD-ESP-BUILT-IN.md
    </Typography>
  </SectionContent>
);

export default RelayBoardWiring;
