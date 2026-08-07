# Relay Board ESP Built-In (ESP-12F 4-Channel)

Integration branch: `RelayBoardEspBuildIn`

## Hardware inventory (from board photos)

| Item | Detail |
|------|--------|
| MCU | DOIT ESP-12F (ESP8266), onboard antenna |
| Relays | 4× Songle SRD-05VDC-SL-C (K1–K4 / RY1–RY4), 10A class |
| Drivers | Transistors Q1–Q4 + flyback diodes D9–D12 (typical) |
| Power | AC L/N terminal **and/or** DC 7–30V → LM2596 → 5V; AMS1117 → 3.3V for ESP |
| Status LED | Red power LED near TXD/GND/5V terminal (hardwired, not GPIO) |
| Relay LEDs | Per-channel indicators tied to relay drive |
| Programming | Header: TX0, RX0, GND, 3V3/5V, IO0; **RST** button |
| Breakouts | IO4, IO5, IO0, IO2, IO15, IO16, IO14, IO12, IO13 |
| Serial adapter path | Laptop USB → Prolific USB-Serial → DB9 → MAX3232/MAX232 board → ESP header |
| Onboard temp sensor | **None** on this PCB |
| Buzzer | Not confirmed on silkscreen; twin exposes optional control only if firmware enables it |

## Default GPIO map (firmware)

LC-style ESP-12F 4-ch boards typically hard-wire:

| Relay | GPIO | Notes |
|-------|------|--------|
| RY1 | 16 | Safe for output |
| RY2 | 14 | |
| RY3 | 12 | |
| RY4 | 13 | |
| Active level | **LOW = ON** | Opto/transistor drive (verify with click test) |

Avoid using GPIO0 / GPIO2 / GPIO15 as relay outputs (boot strapping).

## Wiring diagram — programming / serial (photos)

```mermaid
flowchart LR
  Laptop["Laptop USB"] --> Prolific["Prolific USB-Serial"]
  Prolific --> DB9["DB9 cable"]
  DB9 --> MAX["MAX3232 / MAX232 board"]
  MAX -->|"Yellow → + / 3V3"| ESP3V3["ESP 3V3"]
  MAX -->|"Green → GND"| ESPGND["ESP GND"]
  MAX -->|"Red → TX path"| ESPUART["ESP TX0 / RX0"]
  MAX -->|"Cross TX↔RX as needed"| ESPUART
```

### Header P4 (right of ESP-12F) — typical silk

| Pin | Function |
|-----|----------|
| 3V3 | Logic power (from board or carefully from adapter) |
| TX0 | ESP UART TX |
| RX0 | ESP UART RX |
| IO0 | Pull **LOW** during reset to enter flash mode |
| GND | Common ground |
| 5V | 5V rail (do not feed ESP I/O at 5V) |

**Flash tip:** Hold IO0→GND, press RST (or power-cycle), then upload. Release IO0 after flash starts.

### Windows COM note

PnP may show **Prolific USB-to-Serial Comm Port (COM3)** while pyserial lists the same device as **COM2** if Bluetooth already claimed COM3. Prefer the Prolific VID `067B:2303` port reported by `python -m serial.tools.list_ports`.

## Wiring diagram — power & loads

```mermaid
flowchart TB
  AC["AC L / N"] --> PSU["Onboard AC-DC / transformer"]
  DC["7-30V DC"] --> Buck["LM2596 → 5V"]
  PSU --> Buck
  Buck --> Relays["Relay coils 5V"]
  Buck --> Reg["AMS1117 → 3.3V"]
  Reg --> ESP["ESP-12F"]
  ESP -->|"GPIO 16/14/12/13"| Drivers["Q1–Q4 drivers"]
  Drivers --> Relays
  Relays --> Loads["COM / NO / NC screw terminals"]
```

## Digital twin

In the web UI: **Project → Relay Board Twin**

- CSS-3D board view of relays, LEDs, headers
- Live DO (relays) control via WebSocket
- DI/GPIO legend (boot-sensitive pins marked)
- ESP stats from `/rest/systemStatus` (heap, flash, CPU)
- Wiring tab with this diagram
