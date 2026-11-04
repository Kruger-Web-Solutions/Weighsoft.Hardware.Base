# W-Soft on the small yellow board (`wsoft_cyd28`)

Sunton ESP32-2432S028 ("Cheap Yellow Display", CYD): ESP32-WROOM-32, 4 MB flash, no PSRAM,
2.8" 320x240 panel (ST7789 on the TPM408-2.8 batch, ILI9341 on older ones) with XPT2046 touch (touch not used). Same W-Soft code as the Waveshare
build ([WSOFT-WAVESHARE43.md](WSOFT-WAVESHARE43.md)): approval, Hub queue, DJB poll,
web pages. Only the panel drawing and the pins differ (`WSOFT_BOARD_CYD` in
`lib/wsoft_esp/src/wsoft_screen.cpp`).

> Nothing on this device is legal for trade. Approval stays OFF until Rhyno switches it on
> over USB.

## 1. RS485: needs an add-on

The CYD has no RS485 transceiver. Use a **3.3 V TTL-to-RS485 module with automatic
direction** (no DE/RE pin to drive). Wire it to the **CN1** connector:

| CN1 pin | Module pin |
|---|---|
| 3V3 | VCC |
| GND | GND |
| GPIO22 | the module's TX / TXD / "RO" output (data **from** the box) |
| GPIO27 | the module's RX / RXD / "DI" input (data **to** the box) |

Module A/B go to the junction box A/B. The pins are build flags (`WSOFT_RS485_RX`,
`WSOFT_RS485_TX`); GPIO22/27 are the CN1 pads the `cyd28` env already used for a scale UART.
**Check the module's own labels before power-on**: some modules name TX/RX from their
own side, some from the ESP's side. If the screen stays on `NO SIGNAL`, swap the two data
wires (it cannot damage a 3.3 V module).

Only one RS485 master on the pair: unplug any other reader (Pi, Waveshare) from the box first.

## 1b. RS232 scale instead of the junction box

W-Soft → **Settings** → **Weight comes from**: *Junction box (RS485)* (default) or
*RS232 scale*. Saving a new choice restarts the board (both use the same UART on CN1).

In RS232 mode the existing **Serial reader** (`SerialService`, the same one the other
Hardware.Base boards use) owns the UART on CN1 (`SERIAL2_RX_PIN=22`, `SERIAL2_TX_PIN=27`).
Set the scale's baud, data bits, parity and the **weight pattern** (a regex whose first
group is the number) on the **Serial** page. W-Soft takes that extracted text, parses it
(`wsoft_parse_weight_text`: sign, digits, `.` or `,`, up to 4 decimals; anything else is
not a weight), and runs the same screen, stability and approval checks on it. Unit is kg;
there is no box alarm. A line older than 3 s is `NO SIGNAL`. DJB register jobs
(`/rest/djb`) answer `rs232_mode`.

**The CYD has no RS232 chip.** Its `P1` plug (VIN/TX/RX/GND) is 3.3 V serial and shares
the USB line. A real RS232 scale (up to ±12 V) needs a **3.3 V RS232-to-TTL converter
(MAX3232)** on CN1: converter TTL side to CN1 like the RS485 module above, DB9 side to the
scale.

## 2. Screen (320 x 240)

| Row | Shows |
|---|---|
| Header | `W-SOFT`, Hub state, `RS485 LIVE` / `RS232 LIVE` / `NO SIGNAL` / `DJB ALARM` |
| Middle | Weight (seven-segment font) and unit; gross line and zone (`OK` / `UNDER` / `OVER` / `ALARM`) |
| Band | `TRADE CHECKS ON` (green) or `NOT FOR TRADE` (red) |
| Bottom | Why not for trade (`APPROVAL OFF`, `MOTION`, …) or `SEQ n`; `AUDIT n` |

CYD batches use different panel controllers. The env defaults to the TPM408-2.8 bench
board: `-DST7789_DRIVER=1`, `WSOFT_CYD_ROTATION=3`, `WSOFT_CYD_INVERT=0`.

| What the screen shows | Change |
|---|---|
| Sideways, cut off, a noisy strip on one side | wrong controller: swap `ST7789_DRIVER` / `ILI9341_DRIVER` |
| Dark background, light text | flip `WSOFT_CYD_INVERT` |
| Upside down | `WSOFT_CYD_ROTATION=1` |
| Red and green swapped | add `-DTFT_RGB_ORDER=TFT_BGR` (or `TFT_RGB`) |

First flash (3 Nov 2026) with ILI9341 + invert on drew sideways, clipped, with an 80 px
noise strip and a dark background.

## 3. Build and flash (laptop)

1. `pio test -e native` (host tests, unchanged).
2. `pio run -e wsoft_cyd28`.
3. Plug the CYD's **micro-USB** (CH340, `COM15` on Rhyno's laptop) and flash:
   `pio run -e wsoft_cyd28 -t upload`. This replaces the firmware on the board. Flashing is an
   ASKED gate: only with Rhyno's go.
4. `pio device monitor -b 115200`: expect `weigh screen boot`, `display ready`, then
   `DJB ... LIVE` lines once the RS485 module is wired.
5. Wi-Fi and passwords: same as the Waveshare guide, section 2 step 5 (AP
   `Weighsoft-Esp-NEW`, `http://192.168.4.1`, change the admin and AP passwords).

## 4. Known gaps

- Built in the cloud only; not yet run on a CYD. Bench steps above are still to do.
- The demo target band (1.000–2.000 kg) is the same as on the Waveshare.
- No touch input. The web pages are the way to change settings.
- GPIO2 is the screen's DC line, so the LED example drives the blue RGB LED (GPIO17) here.
- Host tests ran in the cloud with a stand-in Unity (the PlatformIO registry was not
  reachable there); run `pio test -e native` on the laptop for the real runner.
- 4 MB flash: single-app `partitions_ble.csv` (3.25 MB app, no OTA). The ~2.0 MB build does not fit the 1.75 MB slots of `partitions_4mb_ota.csv`. Update over USB.
