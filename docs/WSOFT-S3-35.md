# W-Soft on the S3 with a 3.5" touch screen

Env: `wsoft_s3_35` (`pio run -e wsoft_s3_35`). Same S3-N16R8 board and the same W-Soft code
as `wsoft_s3_headless`, plus the screen. Station model on the Hub: `wsoft-s3-35`.

Picture guide with the wires drawn on the real boards:
https://claude.ai/artifact/DY8FpFxZGPafakF7bgD6NC

## The parts

- **Board:** ESP32-S3 dev board "S3-N16R8 Sixspan", 2 USB-C, no screen (DevKitC-1 pin layout).
- **Screen:** 3.5" module, PCB "3.5in 40PIN SPI Interface 35002 V1.2", box SKU S219C35002A.
  ST7796, 320 x 480, SPI ("3/4-wire"), capacitive touch ("CTP"). The maker's sheet for this
  module family (Elecrow DLS30035B1 / LCD wiki MSP3526) names an **FT6336U** touch chip on I2C.
  This board revision has a different header, so the firmware does not assume the chip: it
  looks at 0x38 (FT5x06 / FT6x36) and 0x5D / 0x14 (GT911) and logs `TOUCH chip at 0x..`.
  If none answers it logs every I2C address it finds.

## Wiring (all on the S3's left row, straight across)

Hold the screen glass up with its header on the right, the S3 to its right. The pins follow the
screen header's order, so no signal wires cross. Only power swaps at the bottom.

| Screen pin (top to bottom) | S3 pin | Wire |
|---|---|---|
| T_IRQ = CTP INT | 7 | light green |
| T_SDO = NC | not wired | |
| T_SDI = CTP SDA | 15 | light blue |
| T_CS = CTP RST | 16 | pink |
| T_CLK = CTP SCL | 8 | brown |
| SDO (MISO) | not wired (the frame lives in PSRAM; the panel is never read) | |
| LED | 9 | white (backlight, high = on, drives the module's own transistor) |
| SCK | 10 | purple |
| SDI (MOSI) | 11 | blue |
| DC | 12 | green |
| RESET | 13 | yellow |
| CS | 14 | orange |
| GND | GND | black |
| VCC | 5V | red (module spec: 5.0 V, on-board level conversion for 3.3 V MCUs; FAQ allows 3.3 V too) |

SPI runs through the GPIO matrix (not the FSPI IOMUX pins 10-13 in their native roles):
fine at 40 MHz.

The SD card header (4 pins) is not used.

Pins that stay as on the headless S3: scale UART RX 18 / TX 17, HX711 DT 1 / SCK 2,
relays 4 / 5 / 6, RGB LED 48, USB 19 / 20. Never use 0, 3, 45, 46 (strapping), 26-32 (flash),
35-37 (octal PSRAM), 43 / 44 (UART0).

## The screen

- 480 x 320 landscape. The W-Soft layout (320 x 240 units) is scaled 1.5 x 1.333, with the
  yellow board's letters (sharper 1.5x letters can be made later with `tools/wsoft-ui/gen.mjs`).
- Rows are drawn into a frame in PSRAM and only the lines of a changed row go to the panel
  (SPI 40 MHz). `SCREEN SHOT` / `SCREEN PIXEL` read that frame.

## If something looks wrong (build flags in `[env:wsoft_s3_35]`)

| What you see | Flag to change |
|---|---|
| Colours look like a photo negative | `-D WSOFT_LCD_INVERT=0` |
| Red and blue swapped | `-D WSOFT_LCD_MADCTL=0x20` (clears BGR) |
| Picture upside down or mirrored | `-D WSOFT_LCD_MADCTL=0xE8` (other landscape) |
| Taps land in the wrong place | each tap logs `TOUCH raw=x,y,addr screen=x,y` on USB; set `WSOFT_TOUCH_SWAP_XY`, `WSOFT_TOUCH_FLIP_X`, `WSOFT_TOUCH_FLIP_Y` |
| White screen, nothing drawn | check CS 14, DC 12, RESET 13, SCK 10, MOSI 11; try `-D WSOFT_LCD_SPI_HZ=20000000` |
| `TOUCH no chip` in the log | check SDA 15 / SCL 8 / RST 16; the log lists any I2C address that answered |

Not bench-tested yet (27 Nov 2026).
