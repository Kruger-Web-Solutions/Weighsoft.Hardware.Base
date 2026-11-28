#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include <gen/wsoft_ui_presets.h>
#include <wsoft_gfx.h>
#include <wsoft_ui_model.h>
#include <wsoft_ui_paint.h>

#include "approval.h"
#include "hilo.h"
#include "hub.h"
#include "loadcell.h"
#include "packers.h"
#include "pin.h"
#include "relay.h"
#include "scale.h"
#include "switches.h"
#include "weighings.h"
#include "wsoft_log.h"
#include "wsoft_ui.h"

#include <time.h>

#ifndef WSOFT_BOARD_HEADLESS
#define WSOFT_BOARD_HEADLESS 0
#endif
#ifndef WSOFT_BOARD_CYD
#define WSOFT_BOARD_CYD 0
#endif
#ifndef WSOFT_BOARD_S3_35
#define WSOFT_BOARD_S3_35 0
#endif

// ======================================================================================
// The panel and touch of each board.
//   gx_begin    start them
//   gx_paint    draw one row of the model on the panel
//   gx_flush    show what was drawn (boards that draw into a frame first)
//   gx_touch    is a finger down, and where (layout units, 320 x 240)
//   gx_capture  hand every dot of a screen to a sink, top to bottom (for SCREEN SHOT / CRC)
//   gx_pixel    one dot as the panel has it (panel dots, not layout units)
// ======================================================================================

static WsoftCanvas canvas;
typedef void (*DotSink)(const uint16_t *dots, size_t count, void *arg);

#if WSOFT_BOARD_HEADLESS
// ---------- no panel ----------
static const int PANEL_W = 0, PANEL_H = 0;
static const uint32_t TAP_GAP_MS = 400;
static bool gx_begin() { return false; }
static void gx_paint(const WsoftUiModel &, int) {}
static void gx_flush() {}
static bool gx_touch(int *, int *) { return false; }
static void gx_capture(const WsoftUiModel *, DotSink, void *) {}
static uint16_t gx_pixel(int, int) { return 0; }

#elif WSOFT_BOARD_CYD
// ---------- yellow board: Sunton ESP32-2432S028 ("CYD"), 2.8" 320 x 240 over SPI ----------
// No PSRAM, so no frame: a row is drawn 16 lines at a time into `band` and pushed to the panel.
// Each band is complete (background first), so nothing flickers.
#include <SPI.h>
#include <TFT_eSPI.h>
#include <gen/wsoft_ui_fonts_1x.h>
#ifndef WSOFT_CYD_ROTATION
#define WSOFT_CYD_ROTATION 3
#endif
#ifndef WSOFT_CYD_INVERT
#define WSOFT_CYD_INVERT 0
#endif
// XPT2046 resistive touch on its own SPI pins (CYD pinout). TFT_eSPI drives the panel on VSPI,
// so touch uses HSPI. Raw readings are mapped to the 320 x 240 screen with these limits; every
// tap logs "TOUCH raw=... screen=..." on USB so a different batch can be calibrated.
// A small XPT2046 reader below (no extra library): the same commands and rotation mapping
// as PaulStoffregen/XPT2046_Touchscreen.
#ifndef WSOFT_TOUCH_CS
#define WSOFT_TOUCH_CS 33
#define WSOFT_TOUCH_IRQ 36
#define WSOFT_TOUCH_CLK 25
#define WSOFT_TOUCH_MISO 39
#define WSOFT_TOUCH_MOSI 32
#endif
#ifndef WSOFT_TOUCH_ROTATION
#define WSOFT_TOUCH_ROTATION WSOFT_CYD_ROTATION
#endif
#ifndef WSOFT_TOUCH_X_MIN
#define WSOFT_TOUCH_X_MIN 200
#define WSOFT_TOUCH_X_MAX 3700
#define WSOFT_TOUCH_Y_MIN 240
#define WSOFT_TOUCH_Y_MAX 3800
#endif

static const int PANEL_W = 320, PANEL_H = 240;
static const int BAND_ROWS = 16;
static const uint32_t TAP_GAP_MS = 600;
static const uint16_t TOUCH_Z_MIN = 400;  // pressure below this is no touch
static TFT_eSPI tft;
static SPIClass touch_spi(HSPI);
static uint16_t band[PANEL_W * BAND_ROWS];
static int touch_raw[3];  // x, y, pressure of the last reading, for the log

static bool gx_begin() {
  tft.init();
  // Rotation 3 is the unmirrored landscape (rotation 1 mirrors). Inversion is set explicitly
  // either way, because CYD batches ship with different panel controllers.
  tft.setRotation(WSOFT_CYD_ROTATION);
  tft.invertDisplay(WSOFT_CYD_INVERT ? true : false);
  tft.fillScreen(0);
  canvas.begin(PANEL_W, PANEL_H, 1, 1, WSOFT_UI_FONTS_1X);
  pinMode(WSOFT_TOUCH_CS, OUTPUT);
  digitalWrite(WSOFT_TOUCH_CS, HIGH);
  pinMode(WSOFT_TOUCH_IRQ, INPUT);
  touch_spi.begin(WSOFT_TOUCH_CLK, WSOFT_TOUCH_MISO, WSOFT_TOUCH_MOSI, -1);
  WSOFT_LOG.println("TOUCH ready");
  return tft.width() == PANEL_W;
}

static void gx_paint(const WsoftUiModel &model, int row) {
  WsoftUiSpan span = wsoft_ui_row_span(row);
  tft.startWrite();
  for (int top = span.y0; top < span.y1; top += BAND_ROWS) {
    int rows = span.y1 - top < BAND_ROWS ? span.y1 - top : BAND_ROWS;
    canvas.window(band, top, rows);
    wsoft_ui_paint_row(canvas, model, row);
    tft.setAddrWindow(0, top, PANEL_W, rows);
    // true: each dot goes out high byte first, as the panel wants it.
    tft.pushColors(band, (uint32_t)PANEL_W * rows, true);
  }
  tft.endWrite();
}

static void gx_flush() {}

// The same bands, to the sink instead of the panel. Nothing on the panel changes.
static void gx_capture(const WsoftUiModel *model, DotSink sink, void *arg) {
  if (model == nullptr) return;
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    if (!wsoft_ui_row_on_page(row, model->page)) continue;
    WsoftUiSpan span = wsoft_ui_row_span(row);
    for (int top = span.y0; top < span.y1; top += BAND_ROWS) {
      int rows = span.y1 - top < BAND_ROWS ? span.y1 - top : BAND_ROWS;
      canvas.window(band, top, rows);
      wsoft_ui_paint_row(canvas, *model, row);
      sink(band, (size_t)PANEL_W * rows, arg);
    }
  }
}

// One dot read back from the panel's own memory: what the glass really got.
static uint16_t gx_pixel(int x, int y) {
  return tft.readPixel(x, y);
}

// One XPT2046 read: pressure, then X and Y (average of two readings after a dropped first one).
static bool gx_touch(int *sx, int *sy) {
  if (digitalRead(WSOFT_TOUCH_IRQ) == HIGH) return false;  // pen IRQ is low while pressed
  touch_spi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(WSOFT_TOUCH_CS, LOW);
  touch_spi.transfer(0xB1);                          // Z1
  int z1 = touch_spi.transfer16(0xC1) >> 3;          // Z2 next
  int z = z1 + 4095;
  int z2 = touch_spi.transfer16(0x91) >> 3;          // X next
  z -= z2;
  touch_spi.transfer16(0x91);                        // first X is noisy: drop it
  int x1 = touch_spi.transfer16(0xD1) >> 3;          // Y next
  int y1 = touch_spi.transfer16(0x91) >> 3;
  int x2 = touch_spi.transfer16(0xD1) >> 3;
  int y2 = touch_spi.transfer16(0xD0) >> 3;          // power down after
  touch_spi.transfer16(0);
  digitalWrite(WSOFT_TOUCH_CS, HIGH);
  touch_spi.endTransaction();
  if (z < TOUCH_Z_MIN) return false;
  int x = (x1 + x2) / 2, y = (y1 + y2) / 2, px, py;
  switch (WSOFT_TOUCH_ROTATION) {
    case 0: px = 4095 - y; py = x; break;
    case 1: px = x; py = y; break;
    case 2: px = y; py = 4095 - x; break;
    default: px = 4095 - x; py = 4095 - y; break;
  }
  touch_raw[0] = px;
  touch_raw[1] = py;
  touch_raw[2] = z;
  *sx = constrain(map(px, WSOFT_TOUCH_X_MIN, WSOFT_TOUCH_X_MAX, 0, PANEL_W - 1), 0, PANEL_W - 1);
  *sy = constrain(map(py, WSOFT_TOUCH_Y_MIN, WSOFT_TOUCH_Y_MAX, 0, PANEL_H - 1), 0, PANEL_H - 1);
  return true;
}

#elif WSOFT_BOARD_S3_35
// ---------- S3 DevKitC + 3.5" SPI module (ST7796, 320 x 480, turned to 480 x 320) ----------
// Module "3.5in 40PIN SPI Interface 35002 V1.2" (SKU S219C35002A); the maker's sheet for this
// family (Elecrow DLS30035B1 / LCD wiki MSP3526) names an FT6336U capacitive touch on I2C.
// The board revision may differ, so the touch chip is found at start: FT5x06/FT6x36 at 0x38 or
// GT911 at 0x5D / 0x14. "TOUCH chip ..." in the USB log says which one answered.
// The panel is write only here (MISO not wired): the rows are drawn into a frame in PSRAM
// (SCREEN SHOT and SCREEN PIXEL read the frame) and the lines of a changed row are sent.
// Pins: docs/WSOFT-S3-35.md. All on the left header of the DevKitC, in the screen header's
// order, so no signal wires cross (SPI goes through the GPIO matrix: fine at 40 MHz).
#include <SPI.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <gen/wsoft_ui_fonts_1x.h>
#ifndef WSOFT_LCD_SCK
#define WSOFT_LCD_SCK 10
#define WSOFT_LCD_MOSI 11
#define WSOFT_LCD_CS 14
#define WSOFT_LCD_DC 12
#define WSOFT_LCD_RST 13
#define WSOFT_LCD_BL 9
#endif
#ifndef WSOFT_CTP_SDA
#define WSOFT_CTP_SDA 15
#define WSOFT_CTP_SCL 8
#define WSOFT_CTP_RST 16
#define WSOFT_CTP_INT 7
#endif
#ifndef WSOFT_LCD_SPI_HZ
#define WSOFT_LCD_SPI_HZ 40000000
#endif
// MADCTL for landscape: 0x28 = MV + BGR (rotation 1), 0xE8 = MX + MY + MV + BGR (rotation 3,
// the picture turned upside down). Clear 0x08 if red and blue are swapped.
#ifndef WSOFT_LCD_MADCTL
#define WSOFT_LCD_MADCTL 0x28
#endif
// IPS panels need the colours inverted; if the screen looks like a photo negative, set 0.
#ifndef WSOFT_LCD_INVERT
#define WSOFT_LCD_INVERT 1
#endif
// Touch to screen: the touch reports the panel's own portrait dots (x 0-319, y 0-479). Every tap
// logs "TOUCH raw=x,y,chip screen=x,y" on USB, so a different batch can be set with these.
#ifndef WSOFT_TOUCH_SWAP_XY
#define WSOFT_TOUCH_SWAP_XY 1
#endif
#ifndef WSOFT_TOUCH_FLIP_X
#define WSOFT_TOUCH_FLIP_X 0
#endif
#ifndef WSOFT_TOUCH_FLIP_Y
#define WSOFT_TOUCH_FLIP_Y 1  // bench 27 Nov: top-left tap read as bottom-left without it
#endif

static const int PANEL_W = 480, PANEL_H = 320;
static const uint32_t TAP_GAP_MS = 400;
static SPIClass lcd_spi(FSPI);
static uint16_t *frame = nullptr;
static uint8_t touch_addr = 0;   // 0: no touch chip answered
static bool touch_gt911 = false;
static bool touch_ok = false;
static int touch_raw[3];          // x, y, chip address of the last reading, for the log

static void lcd_command(uint8_t cmd, const uint8_t *data = nullptr, size_t length = 0) {
  lcd_spi.beginTransaction(SPISettings(WSOFT_LCD_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(WSOFT_LCD_CS, LOW);
  digitalWrite(WSOFT_LCD_DC, LOW);
  lcd_spi.write(cmd);
  if (length > 0) {
    digitalWrite(WSOFT_LCD_DC, HIGH);
    lcd_spi.writeBytes(data, length);
  }
  digitalWrite(WSOFT_LCD_CS, HIGH);
  lcd_spi.endTransaction();
}

static void lcd_cmd(uint8_t cmd, std::initializer_list<uint8_t> data, uint32_t wait_ms = 0) {
  lcd_command(cmd, data.begin(), data.size());
  if (wait_ms) delay(wait_ms);
}

// Panel lines top .. top + rows - 1 of the frame to the glass.
static void lcd_push(int top, int rows) {
  if (rows <= 0) return;
  const uint16_t x1 = PANEL_W - 1, y0 = top, y1 = top + rows - 1;
  lcd_cmd(0x2A, {0, 0, (uint8_t)(x1 >> 8), (uint8_t)x1});
  lcd_cmd(0x2B, {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1});
  lcd_spi.beginTransaction(SPISettings(WSOFT_LCD_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(WSOFT_LCD_CS, LOW);
  digitalWrite(WSOFT_LCD_DC, LOW);
  lcd_spi.write(0x2C);
  digitalWrite(WSOFT_LCD_DC, HIGH);
  // writePixels sends each 16-bit dot high byte first, as the panel wants it.
  lcd_spi.writePixels(frame + (size_t)top * PANEL_W, (uint32_t)PANEL_W * rows * 2);
  digitalWrite(WSOFT_LCD_CS, HIGH);
  lcd_spi.endTransaction();
}

// The ST7796 start-up of TFT_eSPI's ST7796_Init.h (Bodmer, FreeBSD licence), in landscape.
static void lcd_init() {
  digitalWrite(WSOFT_LCD_RST, HIGH);
  delay(5);
  digitalWrite(WSOFT_LCD_RST, LOW);
  delay(20);
  digitalWrite(WSOFT_LCD_RST, HIGH);
  delay(150);
  lcd_cmd(0x01, {}, 120);                 // software reset
  lcd_cmd(0x11, {}, 120);                 // sleep out
  lcd_cmd(0xF0, {0xC3});                  // command set part 1 on
  lcd_cmd(0xF0, {0x96});                  // command set part 2 on
  lcd_cmd(0x36, {WSOFT_LCD_MADCTL});      // memory access: landscape
  lcd_cmd(0x3A, {0x55});                  // 16 bits a dot
  lcd_cmd(0xB4, {0x01});                  // 1-dot inversion
  lcd_cmd(0xB6, {0x80, 0x02, 0x3B});      // display function control
  lcd_cmd(0xE8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33});
  lcd_cmd(0xC1, {0x06});
  lcd_cmd(0xC2, {0xA7});
  lcd_cmd(0xC5, {0x18}, 120);
  lcd_cmd(0xE0, {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B});
  lcd_cmd(0xE1, {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B}, 120);
  lcd_cmd(0xF0, {0x3C});                  // command set part 1 off
  lcd_cmd(0xF0, {0x69}, 120);             // command set part 2 off
  lcd_cmd(WSOFT_LCD_INVERT ? 0x21 : 0x20, {});
  lcd_cmd(0x29, {}, 20);                  // display on
}

static bool i2c_present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool i2c_read(uint8_t addr, const uint8_t *reg, size_t reg_len, uint8_t *out, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg, reg_len);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) out[i] = Wire.read();
  return true;
}

static int touch_sda = WSOFT_CTP_SDA, touch_scl = WSOFT_CTP_SCL;
static bool touch_unknown = false;  // a chip at another address: read it like an FT6336

// The first address that answers on the touch wires (0 if none). Only the touch chip sits there.
static uint8_t i2c_first() {
  for (uint8_t a = 0x08; a < 0x78; a++)
    if (i2c_present(a)) return a;
  return 0;
}

// For a chip at an address the code does not know: what its ID registers hold, so the
// chip can be named from the log. FT-style 8-bit registers and GT911-style 16-bit ones.
static void touch_log_ids() {
  uint8_t d[16];
  const uint8_t r0 = 0x00;
  WSOFT_LOG.printf("TOUCH 0x%02X regs 00-0F:", touch_addr);
  if (i2c_read(touch_addr, &r0, 1, d, 16))
    for (int i = 0; i < 16; i++) WSOFT_LOG.printf(" %02X", d[i]);
  else WSOFT_LOG.print(" (no answer)");
  const uint8_t ids[] = {0xA0, 0xA3, 0xA6, 0xA8};
  for (uint8_t r : ids) {
    if (i2c_read(touch_addr, &r, 1, d, 1)) WSOFT_LOG.printf(" %02X=%02X", r, d[0]);
  }
  const uint8_t gt_id[2] = {0x81, 0x40};
  if (i2c_read(touch_addr, gt_id, 2, d, 4))
    WSOFT_LOG.printf(" 8140=%02X %02X %02X %02X", d[0], d[1], d[2], d[3]);
  WSOFT_LOG.println();
}

// Looks for the touch chip with SDA on `sda` and SCL on `scl`. 100 kHz: the I2C driver turns
// on the S3's own (weak) pull-ups, enough at this speed for a module without its own.
static bool touch_probe(int sda, int scl) {
  Wire.end();
  if (!Wire.begin(sda, scl, 100000)) return false;
  for (int attempt = 0; attempt < 3; attempt++) {
    if (i2c_present(0x38)) touch_addr = 0x38, touch_gt911 = false;
    else if (i2c_present(0x5D)) touch_addr = 0x5D, touch_gt911 = true;
    else if (i2c_present(0x14)) touch_addr = 0x14, touch_gt911 = true;
    else if (sda == WSOFT_CTP_SDA) touch_addr = i2c_first(), touch_gt911 = false;
    touch_unknown = touch_addr != 0 && !touch_gt911 && touch_addr != 0x38;
    if (touch_addr) {
      touch_sda = sda;
      touch_scl = scl;
      return true;
    }
    delay(50);
  }
  return false;
}

// SDA and SCL as wired, then the other way round (two wires swapped on the plug).
static bool touch_find() {
  if (touch_probe(WSOFT_CTP_SDA, WSOFT_CTP_SCL)) return true;
  if (touch_probe(WSOFT_CTP_SCL, WSOFT_CTP_SDA)) return true;
  touch_probe(WSOFT_CTP_SDA, WSOFT_CTP_SCL);  // leave the bus on the planned pins
  return false;
}

static void touch_log_found() {
  WSOFT_LOG.printf("TOUCH chip at 0x%02X (%s), SDA %d, SCL %d%s\n", touch_addr,
                   touch_unknown ? "unknown chip, read like an FT6336"
                   : touch_gt911 ? "GT911"
                                 : "FT5x06/FT6x36",
                   touch_sda, touch_scl,
                   touch_sda != WSOFT_CTP_SDA ? " (SDA and SCL wires are swapped: works anyway)" : "");
  if (touch_unknown) touch_log_ids();
}

static void touch_reset() {
  // Reset with INT held low: a GT911 then takes address 0x5D. An FT6336 ignores INT here.
  pinMode(WSOFT_CTP_INT, OUTPUT);
  digitalWrite(WSOFT_CTP_INT, LOW);
  pinMode(WSOFT_CTP_RST, OUTPUT);
  digitalWrite(WSOFT_CTP_RST, LOW);
  delay(20);
  digitalWrite(WSOFT_CTP_RST, HIGH);
  delay(60);
  pinMode(WSOFT_CTP_INT, INPUT_PULLUP);
  delay(300);  // the FT6336 needs about 300 ms after reset before it answers
}

static void touch_begin() {
  touch_reset();
  touch_ok = touch_find();
  if (touch_ok) {
    touch_log_found();
    return;
  }
  // Nothing at the known addresses: how the lines sit, and whatever answers on the bus.
  Wire.end();
  pinMode(WSOFT_CTP_SDA, INPUT_PULLUP);
  pinMode(WSOFT_CTP_SCL, INPUT_PULLUP);
  delay(2);
  WSOFT_LOG.printf("TOUCH lines: SDA(%d)=%d SCL(%d)=%d INT(%d)=%d (1 = idle high, 0 = held low)\n",
                   WSOFT_CTP_SDA, digitalRead(WSOFT_CTP_SDA), WSOFT_CTP_SCL, digitalRead(WSOFT_CTP_SCL),
                   WSOFT_CTP_INT, digitalRead(WSOFT_CTP_INT));
  Wire.begin(WSOFT_CTP_SDA, WSOFT_CTP_SCL, 100000);
  WSOFT_LOG.print("TOUCH no chip at 0x38/0x5D/0x14; I2C has:");
  for (uint8_t a = 1; a < 127; a++)
    if (i2c_present(a)) WSOFT_LOG.printf(" 0x%02X", a);
  WSOFT_LOG.printf(" (check SDA %d / SCL %d / RST %d); looking again every 5 s\n", WSOFT_CTP_SDA,
                   WSOFT_CTP_SCL, WSOFT_CTP_RST);
}

// While no chip answered: look again every 5 s, so a wire fixed while the board runs is found.
static void touch_retry() {
  static uint32_t last_try = 0;
  if (touch_ok || millis() - last_try < 5000) return;
  last_try = millis();
  touch_reset();
  if (touch_find()) {
    touch_ok = true;
    touch_log_found();
  }
}

static bool gx_begin() {
  pinMode(WSOFT_LCD_CS, OUTPUT);
  digitalWrite(WSOFT_LCD_CS, HIGH);
  pinMode(WSOFT_LCD_DC, OUTPUT);
  pinMode(WSOFT_LCD_RST, OUTPUT);
  pinMode(WSOFT_LCD_BL, OUTPUT);
  digitalWrite(WSOFT_LCD_BL, LOW);  // dark until the first picture is on the glass
  lcd_spi.begin(WSOFT_LCD_SCK, -1, WSOFT_LCD_MOSI, -1);
  const size_t bytes = (size_t)PANEL_W * PANEL_H * sizeof(uint16_t);
  frame = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frame == nullptr) {
    WSOFT_LOG.println("frame alloc failed");
    return false;
  }
  memset(frame, 0, bytes);
  lcd_init();
  lcd_push(0, PANEL_H);
  digitalWrite(WSOFT_LCD_BL, HIGH);  // LED pin high = backlight on (maker's FAQ)
  canvas.begin(PANEL_W, PANEL_H, PANEL_W / 320.0, PANEL_H / 240.0, WSOFT_UI_FONTS_1X);
  canvas.window(frame, 0, PANEL_H);
  touch_begin();
  WSOFT_LOG.println(touch_ok ? "touch ready" : "touch not found yet (no taps)");
  return true;
}

static void gx_paint(const WsoftUiModel &model, int row) {
  canvas.window(frame, 0, PANEL_H);
  wsoft_ui_paint_row(canvas, model, row);
  WsoftUiSpan span = wsoft_ui_row_span(row);
  int top = span.y0 * PANEL_H / 240;
  int end = (span.y1 * PANEL_H + 239) / 240;
  if (top < 0) top = 0;
  if (end > PANEL_H) end = PANEL_H;
  lcd_push(top, end - top);
}

static void gx_flush() {}

static void gx_capture(const WsoftUiModel *model, DotSink sink, void *arg) {
  if (model != nullptr) {
    canvas.window(frame, 0, PANEL_H);
    wsoft_ui_paint(canvas, *model);
  }
  sink(frame, (size_t)PANEL_W * PANEL_H, arg);
}

static uint16_t gx_pixel(int x, int y) {
  return frame[y * PANEL_W + x];
}

// One reading of the first finger, in the panel's portrait dots: 1 a finger, 0 none,
// -1 nothing new (a GT911 with no fresh report: the last answer stands).
static int touch_read(int *rx, int *ry) {
  uint8_t d[5];
  if (touch_gt911) {
    const uint8_t status_reg[2] = {0x81, 0x4E};
    if (!i2c_read(touch_addr, status_reg, 2, d, 1)) return -1;
    if (!(d[0] & 0x80)) return -1;
    int result = 0;
    if ((d[0] & 0x0F) > 0) {
      const uint8_t point_reg[2] = {0x81, 0x50};
      uint8_t p[4];
      if (i2c_read(touch_addr, point_reg, 2, p, 4)) {
        *rx = p[0] | (p[1] << 8);
        *ry = p[2] | (p[3] << 8);
        result = 1;
      }
    }
    Wire.beginTransmission(touch_addr);  // hand the buffer back to the chip
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0);
    Wire.endTransmission();
    return result;
  }
  if (touch_unknown) {  // log regs 00-07 whenever they change, so the chip's map can be read
    static uint8_t seen[8];
    static uint32_t logged = 0;
    uint8_t now[8];
    const uint8_t r0 = 0x00;
    if (i2c_read(touch_addr, &r0, 1, now, 8) && memcmp(now, seen, 8) != 0 &&
        millis() - logged >= 100) {
      memcpy(seen, now, 8);
      logged = millis();
      WSOFT_LOG.printf("TOUCH regs 00-07: %02X %02X %02X %02X %02X %02X %02X %02X\n", now[0], now[1],
                       now[2], now[3], now[4], now[5], now[6], now[7]);
    }
  }
  const uint8_t reg = 0x02;  // TD_STATUS, then P1_XH, P1_XL, P1_YH, P1_YL
  if (!i2c_read(touch_addr, &reg, 1, d, 5)) return 0;
  int count = d[0] & 0x0F;
  if (count < 1 || count > 5) return 0;
  *rx = ((d[1] & 0x0F) << 8) | d[2];
  *ry = ((d[3] & 0x0F) << 8) | d[4];
  return 1;
}

// Read every 30 ms; in between the last answer stands.
static bool gx_touch(int *x, int *y) {
  static uint32_t last_read = 0;
  static bool down = false;
  static int last_x = 0, last_y = 0;
  if (!touch_ok) {
    touch_retry();
    return false;
  }
  if (millis() - last_read >= 30) {
    last_read = millis();
    int rx = 0, ry = 0;
    int got = touch_read(&rx, &ry);
    if (got >= 0) down = got == 1;
    if (got == 1) {
      int px = WSOFT_TOUCH_SWAP_XY ? ry : rx;  // 0 .. 479 across
      int py = WSOFT_TOUCH_SWAP_XY ? rx : ry;  // 0 .. 319 down
      if (WSOFT_TOUCH_FLIP_X) px = PANEL_W - 1 - px;
      if (WSOFT_TOUCH_FLIP_Y) py = PANEL_H - 1 - py;
      px = constrain(px, 0, PANEL_W - 1);
      py = constrain(py, 0, PANEL_H - 1);
      touch_raw[0] = rx;
      touch_raw[1] = ry;
      touch_raw[2] = touch_addr;
      last_x = px * 320 / PANEL_W;
      last_y = py * 240 / PANEL_H;
    }
  }
  *x = last_x;
  *y = last_y;
  return down;
}
#else
// ---------- Waveshare ESP32-S3-Touch-LCD-4.3: 800 x 480 RGB panel, GT911 touch ----------
// The rows are drawn into one frame in PSRAM; the whole frame goes to the panel when
// something was drawn.
#include <driver/gpio.h>
#include <esp_display_panel.hpp>
#include <esp_expander_ch422g.hpp>
#include <esp_heap_caps.h>
#include <gen/wsoft_ui_fonts_2x.h>

using namespace esp_panel::board;
using namespace esp_panel::drivers;

static const int PANEL_W = 800, PANEL_H = 480;
static const uint32_t TAP_GAP_MS = 400;
static Board *board = nullptr;
static LCD *lcd = nullptr;
static Touch *touch = nullptr;
static bool touch_ok = false;
static uint16_t *frame = nullptr;
static bool frame_dirty = false;

static bool gx_begin() {
  board = new Board();
  board->init();
  auto *expander = board->getIO_Expander();
  // After a cold power-on the CH422G can miss the first few I2C writes.
  bool expander_ok = false;
  for (int attempt = 1; expander != nullptr && attempt <= 20 && !expander_ok; attempt++) {
    expander_ok = expander->begin();
    if (!expander_ok) {
      WSOFT_LOG.printf("expander begin failed, attempt %d\n", attempt);
      delay(200);
    }
  }
  if (!expander_ok) {
    WSOFT_LOG.println("expander gave up");
    return false;
  }
  auto *chip = static_cast<esp_expander::CH422G *>(expander->getBase());
  chip->enableAllIO_Output();
  chip->digitalWrite(2, 1);
  // EXIO5 (USB_SEL / CAN_SEL) resets high, which selects CAN and can take over the RS485 GPIO15/16.
  chip->digitalWrite(5, 0);
  chip->digitalWrite(3, 0);
  delay(20);
  chip->digitalWrite(3, 1);
  delay(100);

  lcd = board->getLCD();
  auto *bus = static_cast<BusRGB *>(lcd->getBus());
  if (!bus->configRGB_BounceBufferSize(PANEL_W * 10) || !lcd->begin()) {
    WSOFT_LOG.println("lcd begin failed");
    return false;
  }

  const size_t bytes = (size_t)PANEL_W * PANEL_H * sizeof(uint16_t);
  frame = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frame == nullptr) {
    WSOFT_LOG.println("frame alloc failed");
    lcd->colorBarTest();
    return false;
  }
  memset(frame, 0, bytes);
  canvas.begin(PANEL_W, PANEL_H, 2.5, 2, WSOFT_UI_FONTS_2X);
  canvas.window(frame, 0, PANEL_H);

  // GT911 touch on the shared I2C bus. The same reset steps as the library's board file
  // (TP_INT GPIO4 low, TP_RST on EXIO1). The screen still shows the weight without it.
  touch = board->getTouch();
  if (touch != nullptr) {
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4, 0);
    delay(10);
    chip->digitalWrite(1, 0);
    delay(100);
    chip->digitalWrite(1, 1);
    delay(200);
    gpio_reset_pin(GPIO_NUM_4);
    touch_ok = touch->begin();
  }
  WSOFT_LOG.println(touch_ok ? "touch ready" : "touch failed (no taps)");
  return true;
}

static void gx_paint(const WsoftUiModel &model, int row) {
  canvas.window(frame, 0, PANEL_H);
  wsoft_ui_paint_row(canvas, model, row);
  frame_dirty = true;
}

static void gx_flush() {
  if (!frame_dirty) return;
  frame_dirty = false;
  lcd->drawBitmap(0, 0, PANEL_W, PANEL_H, reinterpret_cast<const uint8_t *>(frame), 1000);
}

// model == nullptr: the frame as it is on the panel now. Else the model is painted into the
// frame (not shown): the caller paints the live screen again afterwards.
static void gx_capture(const WsoftUiModel *model, DotSink sink, void *arg) {
  if (model != nullptr) {
    canvas.window(frame, 0, PANEL_H);
    wsoft_ui_paint(canvas, *model);
  }
  sink(frame, (size_t)PANEL_W * PANEL_H, arg);
}

// One dot of the frame the panel was last handed (this panel cannot be read back).
static uint16_t gx_pixel(int x, int y) {
  return frame[y * PANEL_W + x];
}

// Read every 30 ms; in between the last answer stands.
static bool gx_touch(int *x, int *y) {
  static uint32_t last_read = 0;
  static bool down = false;
  static int last_x = 0, last_y = 0;
  if (!touch_ok) return false;
  if (millis() - last_read >= 30) {
    last_read = millis();
    TouchPoint points[1];
    down = touch->readPoints(points, 1, 0) > 0;
    if (down) {
      last_x = constrain(points[0].x, 0, PANEL_W - 1) * 320 / PANEL_W;
      last_y = constrain(points[0].y, 0, PANEL_H - 1) * 240 / PANEL_H;
    }
  }
  *x = last_x;
  *y = last_y;
  return down;
}
#endif

// ======================================================================================
// The pages and the taps, the same on every board.
// ======================================================================================

static const uint32_t IDLE_MS = 30000;        // back to the live page after 30 s without a tap
static const uint32_t WIFI_IDLE_MS = 180000;  // the Wi-Fi page stays 3 min: the person is on a phone
static const uint32_t WIFI_GRACE_MS = 15000;  // no Wi-Fi alert while the board is still starting
static const uint32_t RESULT_MS = 3000;       // how long SAVE shows its last result
static const char *SCREEN_REASON = "switched on the screen";

static bool ready = false;
static bool dark = true;
static int style = WSOFT_UI_STYLE_CLASSIC;
static int page = WSOFT_UI_PAGE_LIVE;
static WsoftUiView view;           // the newest poll
static WsoftUiModel shown;         // what is on the panel
static uint32_t shown_key[WSOFT_UI_ROW_COUNT];
static bool shown_valid[WSOFT_UI_ROW_COUNT];
static WsoftPinPad pad;
static bool pin_target = false;    // Approval ON (true) or OFF, once the PIN is right
enum PinMode { PIN_APPROVAL = 0, PIN_PACKER, PIN_FLOOR_MGR };
static PinMode pin_mode = PIN_APPROVAL;
static int pin_return_page = WSOFT_UI_PAGE_MENU;
static char pin_packer_code[WSOFT_PACKER_CODE_MAX + 1] = "";
static char pin_packer_name[WSOFT_PACKER_NAME_MAX + 1] = "";
static uint32_t auth_job_id = 0;
static uint32_t auth_job_started_ms = 0;
static PinMode auth_job_mode = PIN_PACKER;
static char auth_job_code[WSOFT_PACKER_CODE_MAX + 1] = "";
static char auth_job_pin[WSOFT_PACKER_PIN_MAX + 1] = "";
static uint8_t picker_page = 0;
static char floor_actor_code[WSOFT_PACKER_CODE_MAX + 1] = "";
static uint8_t floor_index = 0;
static int32_t handin_lost = 0;
static int32_t handin_returned = 0;
static uint8_t handin_reason_idx = 0;
static const char *const HANDIN_REASONS[3] = {"RIPPED", "STOLEN", "OTHER"};
static uint32_t bag_event_seq = 0;
static char message[40] = "";      // the open page's top-right line
static uint32_t last_tap_ms = 0;
// What the last ZERO tap did, shown under ZERO on the live page for RESULT_MS.
static char zero_note[18] = "";
static uint32_t zero_note_ms = 0;
static bool wifi_dismissed = false;
// The copy of the screen in the browser (one screen, two places): the web server's task reads
// the words here and leaves its taps here; only this loop touches the pages.
static portMUX_TYPE web_mux = portMUX_INITIALIZER_UNLOCKED;
static WsoftUiModel web_model;
static bool web_model_valid = false;
static volatile bool web_tap_waiting = false;
static volatile int16_t web_tap_x = 0, web_tap_y = 0;

static bool wifi_up() {
  return WiFi.status() == WL_CONNECTED;
}

// Dark or light and the style of the live page are kept in flash (NVS "wsoft_ui" / "dark" and
// "style"); classic dark until somebody switches it.
static void theme_load() {
  Preferences prefs;
  if (prefs.begin("wsoft_ui", false)) {
    dark = prefs.getBool("dark", true);
    style = prefs.getUChar("style", WSOFT_UI_STYLE_CLASSIC);
    if (style >= WSOFT_UI_STYLE_COUNT) style = WSOFT_UI_STYLE_CLASSIC;
    prefs.end();
  }
}

static void theme_save() {
  Preferences prefs;
  if (prefs.begin("wsoft_ui", false)) {
    prefs.putBool("dark", dark);
    prefs.putUChar("style", (uint8_t)style);
    prefs.end();
  }
}

// "SETUP_INCOMPLETE" -> "SETUP INCOMPLETE".
static void say(const char *text) {
  strlcpy(message, text, sizeof message);
  for (char *p = message; *p; p++) {
    if (*p == '_') *p = ' ';
  }
}

static void open_page(int next) {
  page = next;
  last_tap_ms = millis();
  if (next != WSOFT_UI_PAGE_PIN) {
    if (auth_job_id != 0) {
      hub_packer_auth_cancel(auth_job_id);
      auth_job_id = 0;
      auth_job_code[0] = 0;
      memset(auth_job_pin, 0, sizeof auth_job_pin);
    }
    pad.clear();
    pin_mode = PIN_APPROVAL;
  }
  if (next == WSOFT_UI_PAGE_LIVE) message[0] = 0;
}

// ----- the facts, read from the services -----

// The facts and the texts they point at.
struct Facts {
  WsoftUiInput in;
  WsoftHiloProduct product;
  char ip[20];
  char address[20];
  char ap_name[33];  // 32 characters at most: a longer answer from the Wi-Fi driver is not the name
  char plate_sa[40];
  char plate_limits[56];
  char preset[24];
  char pack_unit[WSOFT_PACKER_UNIT_MAX + 1];
  char floor_code[WSOFT_PACKER_CODE_MAX + 1];
  char floor_name[WSOFT_PACKER_NAME_MAX + 1];
};

static void gather(Facts *f) {
  static const WsoftUiInput EMPTY_INPUT;
  WsoftUiInput &in = f->in;
  in = EMPTY_INPUT;
  in.dark = dark;
  in.style = style;
  in.page = page;

  in.wifi_up = wifi_up();
  bool hotspot = (WiFi.getMode() & WIFI_AP) != 0;
  f->ip[0] = 0;
  if (in.wifi_up) {
    in.rssi = WiFi.RSSI();
    strlcpy(f->ip, WiFi.localIP().toString().c_str(), sizeof f->ip);
  }
  in.ip = f->ip;
  in.wifi_alert = !in.wifi_up && !wifi_dismissed && millis() > WIFI_GRACE_MS;
  HubState hub = hub_state();
  in.hub_online = hub == HUB_ONLINE;
  in.hub_setup = hub == HUB_SETUP;
  in.hub_text = hub_state_text(hub);

  in.source = view.source;
  in.linked = view.linked;
  in.alarm = view.alarm;
  in.stable = approval_stable();
  in.gross = view.gross;
  in.net = view.net;
  in.tared = view.tared;
  in.decimals = view.decimals;
  in.unit = view.unit;
  in.hold = view.hold;

  in.check = view.check;
  in.trade_seq = approval_last_trade_weight().seq;
  in.audit = approval_audit_counter();
  ScaleInfo scale;
  scale_info(&scale);
  f->plate_sa[0] = f->plate_limits[0] = 0;
  if (scale.rs485) {
    wsoft_ui_plate_text(scale.setup, scale.card, f->plate_sa, sizeof f->plate_sa, f->plate_limits, sizeof f->plate_limits);
  }
  in.plate_sa = f->plate_sa;
  in.plate_limits = f->plate_limits;

  // The zone the relays are driven with, so the weight panel and the relays always agree.
  in.zone = relay_zone();
  if (hilo_current(&f->product)) in.product = &f->product;
  HiloInfo hilo;
  hilo_info(&hilo);
  in.product_count = hilo.count;

  WeighingsInfo weigh;
  weighings_info(&weigh);
  in.writer = view.writer;
  in.auto_save = weigh.auto_save;
  in.pending = weigh.pending;
  if (weigh.last_result != WSOFT_SAVE_NONE && millis() - weigh.last_result_ms < RESULT_MS) in.recent = weigh.last_result;
  in.saved_seq = weigh.last.seq;
  in.waiting = weigh.waiting;
  in.countdown = weigh.countdown;

  SwitchesInfo switches;
  switches_info(&switches);
  in.approval_on = switches.approval_on;
  in.rs485 = switches.rs485;
  WsoftTare tare = scale_tare();
  in.tare_on = tare.on;
  in.tare_value = tare.value;
  f->preset[0] = 0;
  // ZERO on every board that weighs itself or reads an RS485 card (Rhyno, 23 Nov 2026: the
  // Waveshare had no ZERO). Not on RS232 (the indicator zeroes) or a Writer (the weigh board does).
  in.zero = strcmp(view.source, "LOADCELL") == 0 || strcmp(view.source, "RS485") == 0;
  in.zero_note = zero_note[0] && millis() - zero_note_ms < RESULT_MS ? zero_note : "";
  in.preset_on = scale.setup.preset_tare_kg != WSOFT_KG_UNSET && scale.setup.preset_tare_kg > 0;
  if (in.preset_on) {
    char kg[16];
    wsoft_kg_text(scale.setup.preset_tare_kg, kg, sizeof kg);
    snprintf(f->preset, sizeof f->preset, "%s kg", kg);
  }
  in.preset_text = f->preset;

  in.message = message;
  in.pin_count = (int)strlen(pad.entry());
  // The address to type on a phone or PC: the Wi-Fi address, else the board's own hotspot.
  // The Wi-Fi page is about joining the hotspot, so it always shows the hotspot's. Both stay
  // empty with no hotspot; the pages then say so. The name is cut at 32 characters: for a
  // name of exactly 32 the Wi-Fi driver's answer runs on into the hotspot password.
  f->address[0] = f->ap_name[0] = 0;
  if (hotspot) {
    strlcpy(f->address, WiFi.softAPIP().toString().c_str(), sizeof f->address);
    strlcpy(f->ap_name, WiFi.softAPSSID().c_str(), sizeof f->ap_name);
  }
  if (page == WSOFT_UI_PAGE_NO_PIN && in.wifi_up) strlcpy(f->address, f->ip, sizeof f->address);
  in.address = f->address;
  in.ap_name = f->ap_name;

  bool packer_on = false;
  f->pack_unit[0] = f->floor_code[0] = f->floor_name[0] = 0;
  packers_ui_snapshot(&packer_on, f->pack_unit, sizeof f->pack_unit, in.strip, &in.strip_count, picker_page,
                      &in.picker_page, &in.picker_pages, in.picker, &in.picker_count);
  picker_page = in.picker_page;
  in.packer_on = packer_on;
  if (packer_on) {
    in.pack_unit = f->pack_unit;
    in.pin_for_packer = (page == WSOFT_UI_PAGE_PIN && pin_mode != PIN_APPROVAL);
    in.pin_for_mgr = (page == WSOFT_UI_PAGE_PIN && pin_mode == PIN_FLOOR_MGR);
    if (page == WSOFT_UI_PAGE_FLOOR || page == WSOFT_UI_PAGE_HANDIN) {
      int32_t fl_issued = 0, fl_rem = 0;
      bool fl_batch = false;
      packers_floor_target(floor_index, &floor_index, &in.floor_total, f->floor_code, sizeof f->floor_code,
                           f->floor_name, sizeof f->floor_name, &fl_issued, &fl_rem, &fl_batch);
      in.floor_code = f->floor_code;
      in.floor_name = f->floor_name;
      in.floor_issued = fl_issued;
      in.floor_remaining = fl_rem;
      in.floor_has_batch = fl_batch;
      in.floor_index = floor_index;
      in.handin_lost = handin_lost;
      in.handin_returned = handin_returned;
      in.handin_reason = HANDIN_REASONS[handin_reason_idx];
    }
  }
}

// Every row is painted again at the next refresh.
static void invalidate() {
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) shown_valid[row] = false;
}

// Builds the model from the facts and paints the rows whose part of it changed.
static void refresh() {
  static Facts facts;
  static WsoftUiModel next;
  gather(&facts);
  wsoft_ui_model(facts.in, &next);
  portENTER_CRITICAL(&web_mux);
  web_model = next;
  web_model_valid = true;
  portEXIT_CRITICAL(&web_mux);
  shown = next;
  if (!ready) return;  // no panel: the browser copy is this board's only screen
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    if (!wsoft_ui_row_on_page(row, next.page)) {
      shown_valid[row] = false;  // painted over by the other page: paint it when it is back
      continue;
    }
    uint32_t key = wsoft_ui_row_key(next, row);
    if (shown_valid[row] && shown_key[row] == key) continue;
    gx_paint(next, row);
    shown_key[row] = key;
    shown_valid[row] = true;
  }
  gx_flush();
}

// ----- what a tap does -----

// Approval was tapped: check what can be checked before asking for the PIN.
static void start_approval() {
  SwitchesInfo info;
  switches_info(&info);
  pin_target = !info.approval_on;
  char line[40];
  if (!info.pin_set) {
    // No PIN is set: say plainly where to set it (Rhyno, 8 Nov 2026).
    message[0] = 0;
    open_page(WSOFT_UI_PAGE_NO_PIN);
  } else if (info.pin_locked_s > 0) {
    snprintf(line, sizeof line, "PIN LOCKED %lu S", (unsigned long)info.pin_locked_s);
    say(line);
  } else if (pin_target && info.blocked != WSOFT_SWITCH_OK) {
    say(wsoft_switch_error_code(info.blocked));
  } else {
    message[0] = 0;
    pin_mode = PIN_APPROVAL;
    pin_return_page = WSOFT_UI_PAGE_MENU;
    open_page(WSOFT_UI_PAGE_PIN);
  }
}

static void start_packer_pin(const char *code, const char *name, int return_page) {
  uint32_t lock_s = packers_guard_locked_s(millis());
  pin_mode = PIN_PACKER;
  pin_return_page = return_page;
  strlcpy(pin_packer_code, code, sizeof pin_packer_code);
  strlcpy(pin_packer_name, name, sizeof pin_packer_name);
  if (lock_s > 0) {
    char line[40];
    snprintf(line, sizeof line, "WAIT %luS", (unsigned long)lock_s);
    say(line);
  } else {
    say(name);
  }
  open_page(WSOFT_UI_PAGE_PIN);
}

static void start_floor_mgr_pin(int return_page) {
  uint32_t lock_s = packers_guard_locked_s(millis());
  pin_mode = PIN_FLOOR_MGR;
  pin_return_page = return_page;
  pin_packer_code[0] = 0;
  pin_packer_name[0] = 0;
  if (lock_s > 0) {
    char line[40];
    snprintf(line, sizeof line, "WAIT %luS", (unsigned long)lock_s);
    say(line);
  } else {
    message[0] = 0;
  }
  open_page(WSOFT_UI_PAGE_PIN);
}

static void finish_packer_online(const WsoftPackerAuthReply &reply) {
  if (reply.ok) {
    packers_apply_auth(reply);
    open_page(WSOFT_UI_PAGE_LIVE);
  } else if (strstr(reply.reason, "LOCK") != nullptr) {
    say("LOCKED");
  } else {
    say("WRONG PIN");
  }
}

static void finish_packer_offline(const char *code, const char *pin) {
  char line[40];
  uint32_t locked_s = 0;
  WsoftPackerPinResult r = packers_sign_in_offline(code, pin, millis(), &locked_s);
  if (r == WSOFT_PACKER_PIN_OK) {
    open_page(WSOFT_UI_PAGE_LIVE);
  } else if (r == WSOFT_PACKER_PIN_LOCKED) {
    snprintf(line, sizeof line, "WAIT %luS", (unsigned long)locked_s);
    say(line);
  } else if (r == WSOFT_PACKER_PIN_WRONG) {
    say("WRONG PIN");
  } else {
    say("SYNC HUB FIRST");
  }
}

static void open_floor_as(const char *actor_code) {
  strlcpy(floor_actor_code, actor_code, sizeof floor_actor_code);
  floor_index = 0;
  handin_lost = 0;
  handin_returned = 0;
  handin_reason_idx = 0;
  message[0] = 0;
  open_page(WSOFT_UI_PAGE_FLOOR);
}

static void finish_floor_online(const WsoftPackerAuthReply &reply) {
  if (reply.ok && reply.floor_manager) {
    packers_apply_auth(reply);
    open_floor_as(reply.code);
  } else if (!reply.ok && strstr(reply.reason, "LOCK") != nullptr) {
    say("LOCKED");
  } else {
    say("WRONG PIN");
  }
}

static void finish_floor_offline(const char *pin) {
  char line[40];
  char actor_code[WSOFT_PACKER_CODE_MAX + 1] = "";
  char actor_name[WSOFT_PACKER_NAME_MAX + 1] = "";
  uint32_t locked_s = 0;
  WsoftPackerPinResult r = packers_verify_manager_pin(pin, millis(), &locked_s, actor_code, sizeof actor_code,
                                                      actor_name, sizeof actor_name);
  if (r == WSOFT_PACKER_PIN_OK) {
    open_floor_as(actor_code);
  } else if (r == WSOFT_PACKER_PIN_LOCKED) {
    snprintf(line, sizeof line, "WAIT %luS", (unsigned long)locked_s);
    say(line);
  } else if (r == WSOFT_PACKER_PIN_WRONG) {
    say("WRONG PIN");
  } else {
    say("NO MGR SET");
  }
}

static void finish_auth_job(HubPackerAuthState st, const WsoftPackerAuthReply &reply) {
  if (auth_job_mode == PIN_PACKER) {
    if (st == HUB_PACKER_AUTH_DONE) {
      finish_packer_online(reply);
    } else {
      finish_packer_offline(auth_job_code, auth_job_pin);
    }
  } else if (auth_job_mode == PIN_FLOOR_MGR) {
    if (st == HUB_PACKER_AUTH_DONE) {
      finish_floor_online(reply);
    } else {
      finish_floor_offline(auth_job_pin);
    }
  }
  auth_job_id = 0;
  auth_job_code[0] = 0;
  memset(auth_job_pin, 0, sizeof auth_job_pin);
}

static void pin_ok() {
  char line[40];
  if (pin_mode == PIN_PACKER || pin_mode == PIN_FLOOR_MGR) {
    if (auth_job_id != 0) return;
    if (!wsoft_packer_pin_format_ok(pad.entry())) {
      say("3-4 DIGITS");
      return;
    }
    uint32_t lock_s = packers_guard_locked_s(millis());
    if (lock_s > 0) {
      pad.clear();
      snprintf(line, sizeof line, "WAIT %luS", (unsigned long)lock_s);
      say(line);
      return;
    }
    char target_code[WSOFT_PACKER_CODE_MAX + 1] = "";
    bool try_online = false;
    if (pin_mode == PIN_PACKER) {
      strlcpy(target_code, pin_packer_code, sizeof target_code);
      try_online = target_code[0] != 0;
    } else {
      try_online = packers_single_manager_code(target_code, sizeof target_code);
    }
    if (try_online) {
      uint32_t jid = hub_packer_auth_start(target_code, pad.entry(), "sign_in", false);
      if (jid != 0) {
        auth_job_id = jid;
        auth_job_started_ms = millis();
        auth_job_mode = pin_mode;
        strlcpy(auth_job_code, target_code, sizeof auth_job_code);
        strlcpy(auth_job_pin, pad.entry(), sizeof auth_job_pin);
        pad.clear();
        say("CHECKING...");
        return;
      }
    }
    char pin_copy[WSOFT_PACKER_PIN_MAX + 1] = "";
    strlcpy(pin_copy, pad.entry(), sizeof pin_copy);
    pad.clear();
    if (pin_mode == PIN_PACKER) {
      finish_packer_offline(target_code, pin_copy);
    } else {
      finish_floor_offline(pin_copy);
    }
    memset(pin_copy, 0, sizeof pin_copy);
    return;
  }

  WsoftPinResult result = pin_try(pad.entry());
  pad.clear();
  if (result == WSOFT_PIN_OK) {
    WsoftSwitchError error =
        switches_apply_here(WSOFT_SWITCH_APPROVAL, pin_target, "PIN", WSOFT_WHERE_SCREEN, SCREEN_REASON);
    open_page(WSOFT_UI_PAGE_MENU);
    say(error == WSOFT_SWITCH_OK ? (pin_target ? "APPROVAL ON" : "APPROVAL OFF") : wsoft_switch_error_code(error));
  } else if (result == WSOFT_PIN_WRONG) {
    snprintf(line, sizeof line, "WRONG PIN %d LEFT", pin_tries_left());
    say(line);
  } else if (result == WSOFT_PIN_LOCKED) {
    open_page(WSOFT_UI_PAGE_MENU);
    snprintf(line, sizeof line, "PIN LOCKED %lu S", (unsigned long)pin_locked_s());
    say(line);
  } else {
    open_page(WSOFT_UI_PAGE_NO_PIN);
  }
}

static void tare(bool preset) {
  WsoftTareError error;
  bool was_on = scale_tare().on;
  if (preset) {
    error = scale_tare_preset(view.decimals, view.unit);
  } else {
    // Stable no longer includes the alarm: keep refusing a tare while the junction box alarms.
    bool stable = view.linked && !view.alarm && approval_stable() && view.hold != WSOFT_HOLD_NOT_STABLE;
    error = scale_tare_toggle(stable, view.gross, view.decimals, view.unit);
  }
  say(error == WSOFT_TARE_OK ? (!preset && was_on ? "TARE CLEARED" : "TARE ON") : wsoft_tare_error_code(error));
}

// ZERO on the menu and the live page (Rhyno, 22 Nov 2026 load cell; 23 Nov 2026 RS485 too): the
// scale drifted off zero, the operator empties it and taps. Load cell: the same zero as on the
// Calibrate tab. RS485 card: the board's own zero (scale_card_zero), nothing is written to the
// card. Both are kept over a restart, both refuse with Approval ON, on a moving reading and
// under a tare (the tare was taken against the old zero).
// What the tap did is said twice: in the menu's message line, and under ZERO on the live page
// for a few seconds (short words: the button holds 17 letters).
static void zero() {
  const char *result;
  if (strcmp(view.source, "RS485") == 0) {
    bool steady = approval_stable() && view.hold != WSOFT_HOLD_NOT_STABLE && !view.alarm;
    result = wsoft_zero_error_screen(scale_card_zero(view.linked, steady, "screen"));
  } else if (approval_on()) result = "APPROVAL IS ON";
  else if (scale_tare().on) result = "CLEAR TARE FIRST";
  else if (!view.linked) result = "CALIBRATE FIRST";
  else if (!approval_stable() || view.hold == WSOFT_HOLD_NOT_STABLE) result = "NOT STEADY";
  else {
    WsoftLoadcellError error = loadcell_set_zero("screen");
    result = error == WSOFT_LOADCELL_OK ? "ZERO SET" : wsoft_loadcell_error_code(error);
  }
  say(result);
  strlcpy(zero_note, message, sizeof zero_note);
  for (char *p = zero_note; *p; p++) *p = (char)toupper((unsigned char)*p);
  zero_note_ms = millis();
}

// PRODUCT on the menu: the next Hi-Lo product (none, first, ..., last, none). The operator's
// pick wins until the Hub sets a new default (Hub ADR 0008, last change wins).
static void next_product() {
  HiloInfo before;
  hilo_info(&before);
  if (before.count == 0) {
    say("SET LIMITS ON THE HUB");
    return;
  }
  hilo_select_next();
  HiloInfo after;
  hilo_info(&after);
  char line[40];
  if (after.picked) snprintf(line, sizeof line, "PRODUCT %s", after.current.code);
  else strlcpy(line, "NO PRODUCT", sizeof line);
  say(line);
}

static void menu_tap(int tile) {
  switch (tile) {
    case WSOFT_UI_TILE_APPROVAL:
      start_approval();
      break;
    case WSOFT_UI_TILE_AUTO: {
      WeighingsInfo weigh;
      weighings_info(&weigh);
      WsoftSwitchError error =
          switches_apply_here(WSOFT_SWITCH_AUTO_SAVE, !weigh.auto_save, "screen", WSOFT_WHERE_SCREEN, "");
      say(error == WSOFT_SWITCH_OK ? (weigh.auto_save ? "AUTO SAVE OFF" : "AUTO SAVE ON") : wsoft_switch_error_code(error));
      break;
    }
    case WSOFT_UI_TILE_TARE:
      if (scale_rs485()) tare(false);
      break;
    case WSOFT_UI_TILE_PRESET:
      if (scale_rs485()) tare(true);
      break;
    case WSOFT_UI_TILE_ZERO:
      if (strcmp(view.source, "LOADCELL") == 0 || strcmp(view.source, "RS485") == 0) zero();
      break;
    case WSOFT_UI_TILE_PRODUCT:
      next_product();
      break;
    case WSOFT_UI_TILE_THEME:
      // One tile steps through the four looks: dark, light, Full Colour, dial.
      if (style == WSOFT_UI_STYLE_CLASSIC && dark) {
        dark = false;
      } else if (style == WSOFT_UI_STYLE_CLASSIC) {
        // Full Colour and dial are dark screens, their menu pages too.
        style = WSOFT_UI_STYLE_COLOUR;
        dark = true;
      } else if (style == WSOFT_UI_STYLE_COLOUR) {
        style = WSOFT_UI_STYLE_DIAL;
      } else {
        style = WSOFT_UI_STYLE_CLASSIC;
        dark = true;
      }
      theme_save();
      say(style == WSOFT_UI_STYLE_COLOUR ? "MINI EDGE" : style == WSOFT_UI_STYLE_DIAL ? "DIAL" : dark ? "DARK MODE" : "LIGHT MODE");
      break;
    case WSOFT_UI_TILE_PACKER:
      message[0] = 0;
      picker_page = 0;
      open_page(WSOFT_UI_PAGE_PACKER);
      break;
    case WSOFT_UI_TILE_WIFI:
      message[0] = 0;
      open_page(WSOFT_UI_PAGE_WIFI);
      break;
    default:
      break;
  }
}

static const char *bag_err_text(WsoftBagEventError err) {
  switch (err) {
    case WSOFT_BAG_OK: return "OK";
    case WSOFT_BAG_ERR_NOT_FLOOR_MANAGER: return "NOT MGR";
    case WSOFT_BAG_ERR_UNKNOWN_PACKER: return "NO PACKER";
    case WSOFT_BAG_ERR_BAD_QTY: return "BAD QTY";
    case WSOFT_BAG_ERR_REASON_REQUIRED: return "NEED REASON";
    case WSOFT_BAG_ERR_EXCEEDS_REMAINING: return "TOO MANY";
    case WSOFT_BAG_ERR_QUEUE_FULL: return "OUTBOX FULL";
    case WSOFT_BAG_ERR_BAD_TEXT: return "BAD TEXT";
  }
  return "ERROR";
}

static void make_bag_event_id(char *out, size_t size) {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  time_t now = time(nullptr);
  snprintf(out, size, "ev-%s-%lu-%lu", mac.c_str(), (unsigned long)(now > 0 ? now : millis()),
           (unsigned long)++bag_event_seq);
}

static const char *make_occurred_at(char *buf, size_t size) {
  time_t now = time(nullptr);
  if (now < 1700000000) return nullptr;
  struct tm utc;
  gmtime_r(&now, &utc);
  strftime(buf, size, "%Y-%m-%dT%H:%M:%SZ", &utc);
  return buf;
}

static int strip_hit_index(int x, int y, uint8_t count) {
  int best = -1;
  int best_dist = 100000;
  for (int i = 0; i < count && i < WSOFT_UI_STRIP_MAX; i++) {
    WsoftUiBox b = wsoft_ui_strip_box(i, count);
    if (!b.hit(x, y)) continue;
    int dx = x - (b.x + b.w / 2);
    int dy = y - (b.y + b.h / 2);
    int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (dist < best_dist) {
      best_dist = dist;
      best = i;
    }
  }
  return best;
}

static void pin_tap(int x, int y) {
  if (wsoft_ui_key_box(3, 1, 1, 1).hit(x, y)) {  // BACK
    int dest = pin_mode == PIN_APPROVAL ? WSOFT_UI_PAGE_MENU : pin_return_page;
    message[0] = 0;
    open_page(dest);
  } else if (wsoft_ui_key_box(3, 0, 1, 1).hit(x, y)) {
    pad.del();
  } else if (wsoft_ui_key_box(3, 2, 1, 2).hit(x, y)) {
    pin_ok();
  } else if (wsoft_ui_key_box(0, 3, 3, 1).hit(x, y)) {
    if (pin_mode == PIN_APPROVAL || strlen(pad.entry()) < WSOFT_PACKER_PIN_MAX) pad.press('0');
  } else {
    for (int i = 0; i < 9; i++) {
      if (!wsoft_ui_key_box(i % 3, i / 3, 1, 1).hit(x, y)) continue;
      if (pin_mode == PIN_APPROVAL || strlen(pad.entry()) < WSOFT_PACKER_PIN_MAX) pad.press((char)('1' + i));
      return;
    }
  }
}

static void packer_tap(int x, int y) {
  if (WSOFT_UI_BOX_PKR_MGR.hit(x, y)) {
    start_floor_mgr_pin(WSOFT_UI_PAGE_PACKER);
    return;
  }
  if (shown.has_active_packer && WSOFT_UI_BOX_PKR_OUT.hit(x, y)) {
    char code[WSOFT_PACKER_CODE_MAX + 1] = "";
    packers_active_code(code, sizeof code);
    if (code[0] != 0) {
      packers_sign_out(code);
      hub_packer_auth_start(code, nullptr, "sign_out", true);
      say("SIGNED OUT");
    }
    return;
  }
  if (shown.picker_pages > 1 && WSOFT_UI_BOX_PKR_PAGE.hit(x, y)) {
    picker_page = (uint8_t)((picker_page + 1) % shown.picker_pages);
    return;
  }
  if (WSOFT_UI_BOX_PKR_BACK.hit(x, y)) {
    open_page(WSOFT_UI_PAGE_LIVE);
    return;
  }
  for (int i = 0; i < shown.picker_count && i < WSOFT_UI_PICKER_MAX; i++) {
    if (!wsoft_ui_tile_box(i).hit(x, y)) continue;
    const WsoftUiPickerEntry &e = shown.picker[i];
    if (e.signed_in) {
      packers_select(e.code, millis());
      open_page(WSOFT_UI_PAGE_LIVE);
    } else {
      start_packer_pin(e.code, e.name, WSOFT_UI_PAGE_PACKER);
    }
    return;
  }
}

static void floor_tap(int x, int y) {
  if (WSOFT_UI_BOX_BACK.hit(x, y)) {
    open_page(WSOFT_UI_PAGE_LIVE);
    return;
  }
  if (WSOFT_UI_BOX_FLR_PICK.hit(x, y) || WSOFT_UI_BOX_FLR_NEXT.hit(x, y)) {
    if (shown.floor_total > 0) {
      floor_index = (uint8_t)((floor_index + 1) % shown.floor_total);
      message[0] = 0;
    }
    return;
  }
  if (WSOFT_UI_BOX_FLR_HANDIN.hit(x, y)) {
    if (shown.floor_total == 0 || shown.floor_code[0] == 0) {
      say("NO PACKERS");
      return;
    }
    handin_lost = 0;
    handin_returned = 0;
    handin_reason_idx = 0;
    char line[40];
    snprintf(line, sizeof line, "%s - %ld LEFT", shown.floor_name, (long)shown.floor_remaining);
    say(line);
    open_page(WSOFT_UI_PAGE_HANDIN);
    return;
  }
  static const int32_t ISSUE_QTY[4] = {25, 50, 100, 200};
  for (int i = 0; i < 4; i++) {
    if (!wsoft_ui_floor_issue_box(i).hit(x, y)) continue;
    if (shown.floor_total == 0 || shown.floor_code[0] == 0) {
      say("NO PACKERS");
      return;
    }
    char ev_id[WSOFT_BAG_EVENT_ID_MAX + 1];
    char occ_buf[WSOFT_BAG_TIME_MAX + 1];
    make_bag_event_id(ev_id, sizeof ev_id);
    const char *occ = make_occurred_at(occ_buf, sizeof occ_buf);
    WsoftBagEventError err = packers_floor_issue(ev_id, floor_actor_code, shown.floor_code, ISSUE_QTY[i], nullptr, occ);
    if (err == WSOFT_BAG_OK) {
      hub_trigger_bag_events_push();
      char line[40];
      snprintf(line, sizeof line, "ISSUED +%ld", (long)ISSUE_QTY[i]);
      say(line);
    } else {
      say(bag_err_text(err));
    }
    return;
  }
}

static void handin_tap(int x, int y) {
  int32_t max_lost = shown.floor_remaining - handin_returned;
  if (max_lost < 0) max_lost = 0;
  int32_t max_ret = shown.floor_remaining - handin_lost;
  if (max_ret < 0) max_ret = 0;

  if (WSOFT_UI_BOX_HI_LOST_DEC.hit(x, y)) {
    if (handin_lost > 0) handin_lost--;
  } else if (WSOFT_UI_BOX_HI_LOST_INC1.hit(x, y)) {
    if (handin_lost + 1 <= max_lost) handin_lost += 1;
  } else if (WSOFT_UI_BOX_HI_LOST_INC5.hit(x, y)) {
    handin_lost += 5;
    if (handin_lost > max_lost) handin_lost = max_lost;
  } else if (WSOFT_UI_BOX_HI_REASON.hit(x, y)) {
    handin_reason_idx = (uint8_t)((handin_reason_idx + 1) % 3);
  } else if (WSOFT_UI_BOX_HI_RET_DEC.hit(x, y)) {
    if (handin_returned > 0) handin_returned--;
  } else if (WSOFT_UI_BOX_HI_RET_INC1.hit(x, y)) {
    if (handin_returned + 1 <= max_ret) handin_returned += 1;
  } else if (WSOFT_UI_BOX_HI_RET_INC10.hit(x, y)) {
    handin_returned += 10;
    if (handin_returned > max_ret) handin_returned = max_ret;
  } else if (WSOFT_UI_BOX_HI_CLOSE.hit(x, y)) {
    char ev_id[WSOFT_BAG_EVENT_ID_MAX + 1];
    char occ_buf[WSOFT_BAG_TIME_MAX + 1];
    make_bag_event_id(ev_id, sizeof ev_id);
    const char *occ = make_occurred_at(occ_buf, sizeof occ_buf);
    const char *reason = handin_lost > 0 ? HANDIN_REASONS[handin_reason_idx] : nullptr;
    WsoftBagEventError err = packers_floor_handin(ev_id, floor_actor_code, shown.floor_code, handin_lost,
                                                  handin_returned, reason, nullptr, true, occ);
    if (err == WSOFT_BAG_OK) {
      hub_trigger_bag_events_push();
      say("BATCH CLOSED");
      open_page(WSOFT_UI_PAGE_FLOOR);
    } else {
      say(bag_err_text(err));
    }
  } else if (WSOFT_UI_BOX_HI_BACK.hit(x, y)) {
    message[0] = 0;
    open_page(WSOFT_UI_PAGE_FLOOR);
  }
}

// x, y in layout units. Works on what is on the panel now (`shown`).
static void tap(int x, int y) {
  last_tap_ms = millis();
  // The Wi-Fi part of the status bar opens the Wi-Fi page from any page.
  if (page != WSOFT_UI_PAGE_WIFI && WSOFT_UI_BOX_WIFI_BAR.hit(x, y)) {
    message[0] = 0;
    open_page(WSOFT_UI_PAGE_WIFI);
    return;
  }
  switch (page) {
    case WSOFT_UI_PAGE_LIVE:
      if (shown.slot == WSOFT_UI_SLOT_WIFI && WSOFT_UI_BOX_CONNECT.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_WIFI);
      } else if (shown.slot == WSOFT_UI_SLOT_WIFI && WSOFT_UI_BOX_DISMISS.hit(x, y)) {
        wifi_dismissed = true;
      } else if (shown.slot == WSOFT_UI_SLOT_STOP && WSOFT_UI_BOX_SLOT.hit(x, y)) {
        // Packer stop -> open Packer picker; Clock not set (when Wi-Fi is down) -> open Wi-Fi.
        WeighingsInfo weigh;
        weighings_info(&weigh);
        if (weigh.waiting == WSOFT_SAVE_NEED_PACKER || weigh.waiting == WSOFT_SAVE_NO_BAGS) {
          message[0] = 0;
          picker_page = 0;
          open_page(WSOFT_UI_PAGE_PACKER);
        } else if (!shown.wifi_up && weigh.waiting == WSOFT_SAVE_NO_CLOCK) {
          open_page(WSOFT_UI_PAGE_WIFI);
        }
      } else if (shown.slot == WSOFT_UI_SLOT_NONE && shown.packer_on && WSOFT_UI_BOX_STRIP_ADD.hit(x, y) &&
                 (!WSOFT_UI_BOX_STRIP_MGR.hit(x, y) || y < 41)) {
        message[0] = 0;
        picker_page = 0;
        open_page(WSOFT_UI_PAGE_PACKER);
      } else if (shown.slot == WSOFT_UI_SLOT_NONE && shown.packer_on && WSOFT_UI_BOX_STRIP_MGR.hit(x, y)) {
        start_floor_mgr_pin(WSOFT_UI_PAGE_LIVE);
      } else if (shown.slot == WSOFT_UI_SLOT_NONE && shown.packer_on && shown.strip_count == 0 &&
                 WSOFT_UI_BOX_STRIP_SIGNIN.hit(x, y)) {
        message[0] = 0;
        picker_page = 0;
        open_page(WSOFT_UI_PAGE_PACKER);
      } else if (shown.slot == WSOFT_UI_SLOT_NONE && shown.packer_on && shown.strip_count > 0 &&
                 strip_hit_index(x, y, shown.strip_count) >= 0) {
        int idx = strip_hit_index(x, y, shown.strip_count);
        packers_select(shown.strip[idx].code, millis());
      } else if (shown.slot == WSOFT_UI_SLOT_NONE && !shown.packer_on && !shown.trade_ok && WSOFT_UI_BOX_STAR.hit(x, y)) {
        // The not-for-trade star (Rhyno, 14 Nov 2026): a tap says what it means.
        say("NOT TRADE APPROVED");
      } else if (x >= WSOFT_UI_BOX_SAVE.x - 4 && y >= WSOFT_UI_BOX_SAVE.y - 4) {
        // SAVE reaches to the corner of the screen: a tap near the edge still saves.
        weighings_request_save();
      } else if (WSOFT_UI_BOX_MENU.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_MENU);
      } else if (shown.zero_btn && WSOFT_UI_BOX_ZERO.hit(x, y)) {
        zero();
        message[0] = 0;  // the live page has no message line; the button says it
      }
      return;
    case WSOFT_UI_PAGE_MENU:
      if (WSOFT_UI_BOX_BACK.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_LIVE);
        return;
      }
      for (int i = 0; i < shown.tile_count && i < WSOFT_UI_TILES_MAX; i++) {
        if (!wsoft_ui_tile_box(i).hit(x, y)) continue;
        menu_tap(shown.tiles[i].id);
        return;
      }
      return;
    case WSOFT_UI_PAGE_PIN:
      pin_tap(x, y);
      return;
    case WSOFT_UI_PAGE_PACKER:
      packer_tap(x, y);
      return;
    case WSOFT_UI_PAGE_FLOOR:
      floor_tap(x, y);
      return;
    case WSOFT_UI_PAGE_HANDIN:
      handin_tap(x, y);
      return;
    case WSOFT_UI_PAGE_NO_PIN:
      open_page(WSOFT_UI_PAGE_MENU);  // a tap anywhere closes the note
      return;
    default:  // the Wi-Fi page
      if (WSOFT_UI_BOX_WIDE.hit(x, y)) open_page(WSOFT_UI_PAGE_LIVE);
      return;
  }
}

bool ui_begin() {
  theme_load();
  ready = gx_begin();
  return ready;
}

void ui_release_touch_bus() {
// Only the Waveshare shares its touch I2C with the load cell; the S3 3.5" keeps touch on.
#if !WSOFT_BOARD_HEADLESS && !WSOFT_BOARD_CYD && !WSOFT_BOARD_S3_35
  touch_ok = false;  // gx_touch() reads nothing more, so no I2C transfer starts again
  gpio_reset_pin(GPIO_NUM_8);  // SDA: input with pull-up, never driven
  WSOFT_LOG.println("touch off: the load cell uses the I2C plug (use the browser Live tab)");
#endif
}

void ui_show(const WsoftUiView &next) {
  view = next;
  refresh();
}

bool ui_model_json(char *out, size_t size) {
  // Two tasks ask now: the web server (the browser's copy of the screen) and the hub task (the
  // Hub's Control room). The copy buffer is shared, so one caller at a time.
  static SemaphoreHandle_t json_lock = xSemaphoreCreateMutex();
  static WsoftUiModel copy;
  if (!json_lock || xSemaphoreTake(json_lock, pdMS_TO_TICKS(500)) != pdTRUE) return false;
  portENTER_CRITICAL(&web_mux);
  bool valid = web_model_valid;
  if (valid) copy = web_model;
  portEXIT_CRITICAL(&web_mux);
  bool ok = valid && wsoft_ui_model_json(copy, out, size) > 0;
  xSemaphoreGive(json_lock);
  return ok;
}

bool ui_remote_tap(int x, int y) {
  if (x < 0 || x >= 320 || y < 0 || y >= 240) return false;
  portENTER_CRITICAL(&web_mux);
  web_tap_x = (int16_t)x;
  web_tap_y = (int16_t)y;
  web_tap_waiting = true;
  portEXIT_CRITICAL(&web_mux);
  return true;
}

void ui_loop() {
  if (auth_job_id != 0) {
    last_tap_ms = millis();
    static WsoftPackerAuthReply auth_reply;
    HubPackerAuthState st = hub_packer_auth_poll(auth_job_id, &auth_reply);
    if (st == HUB_PACKER_AUTH_PENDING && millis() - auth_job_started_ms > 4000) {
      hub_packer_auth_cancel(auth_job_id);
      st = HUB_PACKER_AUTH_OFFLINE;
    }
    if (st != HUB_PACKER_AUTH_PENDING) {
      finish_auth_job(st, auth_reply);
      refresh();
    }
  }
  // A tap from the browser copy: the same tap() the panel's touch calls.
  if (web_tap_waiting) {
    portENTER_CRITICAL(&web_mux);
    int web_x = web_tap_x, web_y = web_tap_y;
    web_tap_waiting = false;
    portEXIT_CRITICAL(&web_mux);
    WSOFT_LOG.printf("TOUCH web=%d,%d\n", web_x, web_y);
    tap(web_x, web_y);
    refresh();
  }
  // One tap per press.
  static bool was_down = false;
  static uint32_t last_press = 0;
  int x = 0, y = 0;
  bool down = ready && gx_touch(&x, &y);
  if (down && !was_down && millis() - last_press > TAP_GAP_MS) {
    last_press = millis();
#if WSOFT_BOARD_CYD || WSOFT_BOARD_S3_35
    WSOFT_LOG.printf("TOUCH raw=%d,%d,%d screen=%d,%d\n", touch_raw[0], touch_raw[1], touch_raw[2], x, y);
#else
    WSOFT_LOG.printf("TOUCH screen=%d,%d\n", x, y);
#endif
    tap(x, y);
    refresh();
  }
  was_down = down;
  if (wifi_up()) wifi_dismissed = false;  // the alert comes back the next time Wi-Fi drops
  if (page != WSOFT_UI_PAGE_LIVE && millis() - last_tap_ms > (page == WSOFT_UI_PAGE_WIFI ? WIFI_IDLE_MS : IDLE_MS)) {
    open_page(WSOFT_UI_PAGE_LIVE);
    refresh();
  }
}

// ======================================================================================
// USB technician commands: SCREEN CRC, SCREEN SHOT [n], SCREEN TAP x y
// ======================================================================================

static void crc_sink(const uint16_t *dots, size_t count, void *arg) {
  uint32_t *crc = static_cast<uint32_t *>(arg);
  *crc = wsoft_crc32(*crc, reinterpret_cast<const uint8_t *>(dots), count * 2);
}

// Run-length text: lines of "SCR <count>*<colour> ..." (hex). Each line goes out in one write,
// so a log line from another task cannot land inside it.
struct ShotSink {
  Stream *port;
  uint32_t crc;
  uint32_t run;
  uint16_t color;
  char line[120];
  size_t used;
  uint32_t lines;
};

static void shot_line(ShotSink *s) {
  if (s->used == 0) return;
  s->line[s->used++] = '\n';
  s->port->write(reinterpret_cast<const uint8_t *>(s->line), s->used);
  s->used = 0;
  if (++s->lines % 32 == 0) delay(1);  // let the other tasks run
}

static void shot_run(ShotSink *s) {
  if (s->run == 0) return;
  if (s->used == 0) s->used = (size_t)snprintf(s->line, sizeof s->line, "SCR");
  s->used += (size_t)snprintf(s->line + s->used, sizeof s->line - s->used, " %lx*%04x", (unsigned long)s->run, s->color);
  s->run = 0;
  if (s->used > sizeof s->line - 20) shot_line(s);
}

static void shot_sink(const uint16_t *dots, size_t count, void *arg) {
  ShotSink *s = static_cast<ShotSink *>(arg);
  s->crc = wsoft_crc32(s->crc, reinterpret_cast<const uint8_t *>(dots), count * 2);
  for (size_t i = 0; i < count; i++) {
    if (s->run > 0 && dots[i] == s->color) {
      s->run++;
      continue;
    }
    shot_run(s);
    s->color = dots[i];
    s->run = 1;
  }
}

void ui_handle_command(const String &command, Stream &port) {
  static WsoftUiModel preset;
  if (!ready) {
    port.println("SCREEN_ERR no screen on this board");
    return;
  }
  if (command == "SCREEN CRC") {
    for (int n = 0; n < WSOFT_UI_PRESET_COUNT; n++) {
      uint32_t crc = 0;
      wsoft_ui_preset(n, &preset);
      gx_capture(&preset, crc_sink, &crc);
      port.printf("SCREEN_CRC %d %08lX\n", n, (unsigned long)crc);
      delay(1);
    }
    port.println("SCREEN_OK crc");
    invalidate();  // a board with a frame painted the test screens into it: paint the live screen again
  } else if (command.startsWith("SCREEN SHOT")) {
    String rest = command.substring(11);
    rest.trim();
    const WsoftUiModel *model = &shown;
    if (rest.length() > 0) {
      if (!wsoft_ui_preset(rest.toInt(), &preset) || !isDigit(rest[0])) {
        port.println("SCREEN_ERR no such test screen");
        return;
      }
      model = &preset;
    }
#if !WSOFT_BOARD_CYD
    // A board with a frame sends the frame itself: exactly what is on the panel. If a test
    // screen was just painted into it (SCREEN CRC or SHOT n in the same burst), paint the
    // live screen back first.
    if (rest.length() == 0) {
      refresh();
      model = nullptr;
    }
#endif
    static ShotSink sink;
    memset(&sink, 0, sizeof sink);
    sink.port = &port;
    port.printf("SCREEN_BEGIN %d %d\n", PANEL_W, PANEL_H);
    gx_capture(model, shot_sink, &sink);
    shot_run(&sink);
    shot_line(&sink);
    port.printf("SCREEN_END %08lX\n", (unsigned long)sink.crc);
    if (model == &preset) invalidate();
  } else if (command.startsWith("SCREEN TAP ")) {
    int x = -1, y = -1;
    if (sscanf(command.c_str() + 11, "%d %d", &x, &y) != 2 || x < 0 || x >= 320 || y < 0 || y >= 240) {
      port.println("SCREEN_ERR use SCREEN TAP x y, x 0-319, y 0-239");
      return;
    }
    tap(x, y);
    refresh();
    port.printf("SCREEN_OK tap %d %d page=%d\n", x, y, page);
  } else if (command.startsWith("SCREEN PIXEL ")) {
    int x = -1, y = -1;
    if (sscanf(command.c_str() + 13, "%d %d", &x, &y) != 2 || x < 0 || x >= PANEL_W || y < 0 || y >= PANEL_H) {
      port.printf("SCREEN_ERR use SCREEN PIXEL x y, x 0-%d, y 0-%d\n", PANEL_W - 1, PANEL_H - 1);
      return;
    }
    port.printf("SCREEN_PIXEL %d %d %04X\n", x, y, gx_pixel(x, y));
  } else {
    port.println("SCREEN_ERR use SCREEN CRC, SCREEN SHOT [n], SCREEN PIXEL x y or SCREEN TAP x y");
  }
}
