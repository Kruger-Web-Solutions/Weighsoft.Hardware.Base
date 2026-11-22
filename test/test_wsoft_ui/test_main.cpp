// Host tests for the board screen: the drawing engine (lib/wsoft_core/src/wsoft_gfx.cpp)
// and what goes where (wsoft_ui_paint.cpp).
// Run: pio test -e native -f test_wsoft_ui   (needs a host g++ on PATH)
//
// The big check: every test screen (gen/wsoft_ui_presets.h) is painted on both panel sizes
// and its CRC-32 must equal the one the reference drawing gives (golden.h, written by
// tools/wsoft-ui/gen.mjs from tools/wsoft-ui/ref.js). A screen that fails is written to
// .pio/wsoft-ui-fail-<panel>-<n>.bin (16-bit dots, low byte first) to compare with the
// picture in docs/img/wsoft-screen.

#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "gen/wsoft_ui_colors.h"
#include "gen/wsoft_ui_fonts_1x.h"
#include "gen/wsoft_ui_fonts_2x.h"
#include "gen/wsoft_ui_presets.h"
#include "golden.h"
#include "wsoft_gfx.h"
#include "wsoft_ui_model.h"
#include "wsoft_ui_paint.h"

void setUp() {}
void tearDown() {}

static const uint16_t WHITE = 0xFFFF;
static const uint16_t GUARD = 0xBEEF;

void test_blend_rounds_each_channel() {
  TEST_ASSERT_EQUAL_HEX16(0x1234, wsoft_blend565(0x1234, 0xFFFF, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, wsoft_blend565(0x1234, 0xFFFF, 255));
  // Half white on black: 31 * 128 / 255 = 15.56 -> 16, 63 * 128 / 255 = 31.6 -> 32.
  TEST_ASSERT_EQUAL_HEX16(0x8410, wsoft_blend565(0x0000, 0xFFFF, 128));
  // Nearly none of red on blue stays blue: nothing leaks into the other channels.
  TEST_ASSERT_EQUAL_HEX16(0x001F, wsoft_blend565(0x001F, 0xF800, 1));
}

void test_crc32_is_the_zip_one() {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, wsoft_crc32(0, (const uint8_t *)"123456789", 9));
  // In two parts gives the same.
  uint32_t part = wsoft_crc32(0, (const uint8_t *)"1234", 4);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, wsoft_crc32(part, (const uint8_t *)"56789", 5));
}

void test_a_rect_stays_inside_its_window() {
  // An 8 x 6 panel; the window is lines 2 and 3. Guard dots sit before and after it.
  uint16_t buffer[8 * 2 + 16];
  for (size_t i = 0; i < sizeof buffer / sizeof buffer[0]; i++) buffer[i] = GUARD;
  WsoftCanvas c;
  c.begin(8, 6, 1, 1, WSOFT_UI_FONTS_1X);
  c.window(buffer + 8, 2, 2);
  c.rect(-3, -3, 40, 40, WHITE);
  for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_HEX16(GUARD, buffer[i]);
  for (int i = 8; i < 24; i++) TEST_ASSERT_EQUAL_HEX16(WHITE, buffer[i]);
  for (int i = 24; i < 32; i++) TEST_ASSERT_EQUAL_HEX16(GUARD, buffer[i]);
}

void test_a_rect_scales_with_the_panel() {
  // Layout box 2,1 4 x 2 on a 2.5 x 2 panel covers dots 5..14 and lines 2..5.
  static uint16_t frame[20 * 8];
  memset(frame, 0, sizeof frame);
  WsoftCanvas c;
  c.begin(20, 8, 2.5, 2, WSOFT_UI_FONTS_2X);
  c.window(frame, 0, 8);
  c.rect(2, 1, 4, 2, WHITE);
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 20; x++) {
      bool inside = x >= 5 && x < 15 && y >= 2 && y < 6;
      TEST_ASSERT_EQUAL_HEX16(inside ? WHITE : 0, frame[y * 20 + x]);
    }
  }
}

void test_round_corners_cut_the_corner_dots() {
  static uint16_t frame[16 * 16];
  memset(frame, 0, sizeof frame);
  WsoftCanvas c;
  c.begin(16, 16, 1, 1, WSOFT_UI_FONTS_1X);
  c.window(frame, 0, 16);
  c.rect(0, 0, 16, 16, WHITE, 8);  // a full circle
  TEST_ASSERT_EQUAL_HEX16(0, frame[0]);                  // the corner dot is outside
  TEST_ASSERT_EQUAL_HEX16(0, frame[15 * 16 + 15]);
  TEST_ASSERT_EQUAL_HEX16(WHITE, frame[8 * 16 + 8]);     // the middle is solid
  TEST_ASSERT_EQUAL_HEX16(WHITE, frame[1 * 16 + 8]);     // just under the top of the circle
  TEST_ASSERT_NOT_EQUAL(0, frame[0 * 16 + 8]);            // the top dot itself is nearly full
  // The edge is smoothed: some dot on the rim is neither black nor white.
  int soft = 0;
  for (int i = 0; i < 16 * 16; i++) {
    if (frame[i] != 0 && frame[i] != WHITE) soft++;
  }
  TEST_ASSERT_TRUE(soft > 8);
  // A pill (r below 0) on a wide box is rounded by half its height.
  memset(frame, 0, sizeof frame);
  c.rect(0, 4, 16, 8, WHITE, -1);
  TEST_ASSERT_EQUAL_HEX16(0, frame[4 * 16 + 0]);
  TEST_ASSERT_EQUAL_HEX16(WHITE, frame[8 * 16 + 1]);
  TEST_ASSERT_EQUAL_HEX16(WHITE, frame[8 * 16 + 14]);
}

// The ring looks only at the dots near its piece (fast enough for the dial on the yellow
// board). It must paint exactly what looking at every dot paints.
static void ring_every_dot(uint16_t *fb, int W, int H, double sx, double sy, double cx, double cy, double r, double w,
                           double ax, double ay, double bx, double by, uint16_t color) {
  double X = cx * sx, Y = cy * sy, R = r * sy, I = R - w * sy;
  for (int py = 0; py < H; py++) {
    for (int px = 0; px < W; px++) {
      double dx = px + 0.5 - X, dy = py + 0.5 - Y, d = sqrt(dx * dx + dy * dy);
      double cover = R - d + 0.5;
      double inner = d - I + 0.5, from = ax * dy - ay * dx + 0.5, to = dx * by - dy * bx + 0.5;
      if (inner < cover) cover = inner;
      if (from < cover) cover = from;
      if (to < cover) cover = to;
      if (cover <= 0) continue;
      int alpha = cover < 1 ? (int)floor(cover * 255 + 0.5) : 255;
      uint16_t &dot = fb[py * W + px];
      dot = alpha >= 255 ? color : wsoft_blend565(dot, color, alpha);
    }
  }
}

void test_a_ring_paints_the_same_as_looking_at_every_dot() {
  static uint16_t fast[800 * 480], slow[800 * 480];
  const double PI2 = 6.283185307179586;
  struct Panel { int W, H; double sx, sy; } panels[] = {{320, 240, 1, 1}, {800, 480, 2.5, 2}};
  int pieces = 0;
  for (const Panel &p : panels) {
    // Pieces all round the circle, short and nearly half a turn, thin and thick, part off the panel.
    for (int k = 0; k < 24; k++) {
      double a = k * PI2 / 24, span = 0.2 + (k % 5) * 0.55;
      double ax = cos(a), ay = sin(a), bx = cos(a + span), by = sin(a + span);
      double cx = 40 + (k * 37) % 260, cy = 30 + (k * 53) % 200, r = 20 + (k * 29) % 140, w = 3 + k % 4 * 4;
      memset(fast, 0, sizeof fast);
      memset(slow, 0, sizeof slow);
      WsoftCanvas c;
      c.begin(p.W, p.H, p.sx, p.sy, WSOFT_UI_FONTS_1X);
      c.window(fast, 0, p.H);
      c.ring(cx, cy, r, w, ax, ay, bx, by, 0xF800);
      ring_every_dot(slow, p.W, p.H, p.sx, p.sy, cx, cy, r, w, ax, ay, bx, by, 0xF800);
      TEST_ASSERT_EQUAL_MEMORY(slow, fast, sizeof(uint16_t) * p.W * p.H);
      pieces++;
    }
  }
  TEST_ASSERT_EQUAL(48, pieces);
}

void test_text_width_and_alignment() {
  static uint16_t frame[320 * 40];
  memset(frame, 0, sizeof frame);
  WsoftCanvas c;
  c.begin(320, 40, 1, 1, WSOFT_UI_FONTS_1X);
  c.window(frame, 0, 40);
  TEST_ASSERT_EQUAL(0, c.dots("", WSOFT_UI_SMALL));
  int menu = c.dots("MENU", WSOFT_UI_SMALL);
  TEST_ASSERT_TRUE(menu > 20 && menu < 60);
  // A character the letters do not hold takes no room and draws nothing.
  TEST_ASSERT_EQUAL(menu, c.dots("ME\x01NU\xC3\xA9", WSOFT_UI_SMALL));
  // The weight letters hold digits, '.', '-' only.
  TEST_ASSERT_EQUAL(0, c.dots("kg", WSOFT_UI_HERO));
  TEST_ASSERT_TRUE(c.dots("50.120", WSOFT_UI_HERO) > 150);
  // Right aligned text ends at its x; nothing is drawn right of it.
  double w = c.text("MENU", 200, 20, WSOFT_UI_SMALL, WHITE, WSOFT_RIGHT);
  TEST_ASSERT_EQUAL_DOUBLE((double)menu, w);
  int leftmost = 320, rightmost = -1;
  for (int y = 0; y < 40; y++) {
    for (int x = 0; x < 320; x++) {
      if (frame[y * 320 + x] == 0) continue;
      if (x < leftmost) leftmost = x;
      if (x > rightmost) rightmost = x;
    }
  }
  TEST_ASSERT_TRUE(rightmost < 200 && rightmost >= 196);
  TEST_ASSERT_TRUE(leftmost >= 200 - menu - 1);
}

void test_marker_position_on_the_bar() {
  // Limits 49 and 51 kg sit at 6/22 and 16/22 of the bar, the target in the middle.
  TEST_ASSERT_EQUAL(273, wsoft_ui_pointer(49000, 49000, 51000));
  TEST_ASSERT_EQUAL(500, wsoft_ui_pointer(50000, 49000, 51000));
  TEST_ASSERT_EQUAL(727, wsoft_ui_pointer(51000, 49000, 51000));
  // Far outside pins to the ends.
  TEST_ASSERT_EQUAL(0, wsoft_ui_pointer(0, 49000, 51000));
  TEST_ASSERT_EQUAL(0, wsoft_ui_pointer(-500, 49000, 51000));
  TEST_ASSERT_EQUAL(1000, wsoft_ui_pointer(60000, 49000, 51000));
  // One-point limits: left, middle or right.
  TEST_ASSERT_EQUAL(0, wsoft_ui_pointer(999, 1000, 1000));
  TEST_ASSERT_EQUAL(500, wsoft_ui_pointer(1000, 1000, 1000));
  TEST_ASSERT_EQUAL(1000, wsoft_ui_pointer(1001, 1000, 1000));
  // The largest weights the Hub sends do not overflow.
  TEST_ASSERT_EQUAL(500, wsoft_ui_pointer(950000000, 900000000, 1000000000));
}

void test_tap_boxes() {
  TEST_ASSERT_TRUE(WSOFT_UI_BOX_SAVE.hit(256, 213));
  TEST_ASSERT_TRUE(WSOFT_UI_BOX_MENU.hit(156, 213));
  // GROSS is a read-out between nothing and MENU: a tap on it is neither MENU nor SAVE.
  TEST_ASSERT_FALSE(WSOFT_UI_BOX_MENU.hit(60, 213));
  TEST_ASSERT_FALSE(WSOFT_UI_BOX_SAVE.hit(60, 213));
  // The gap between MENU and SAVE belongs to neither, even with the 3-unit slop.
  TEST_ASSERT_FALSE(WSOFT_UI_BOX_MENU.hit(196, 213));
  TEST_ASSERT_FALSE(WSOFT_UI_BOX_SAVE.hit(196, 213));
  // Menu tiles: two columns, four rows; the PIN pad's OK key is two rows high.
  TEST_ASSERT_TRUE(wsoft_ui_tile_box(0).hit(20, 60));
  TEST_ASSERT_TRUE(wsoft_ui_tile_box(3).hit(300, 110));
  TEST_ASSERT_FALSE(wsoft_ui_tile_box(0).hit(170, 60));
  TEST_ASSERT_TRUE(wsoft_ui_key_box(3, 2, 1, 2).hit(280, 150));
  TEST_ASSERT_TRUE(wsoft_ui_key_box(3, 2, 1, 2).hit(280, 220));
  TEST_ASSERT_TRUE(wsoft_ui_key_box(0, 3, 3, 1).hit(120, 210));
}

// ---------- from the facts to the words on the screen (wsoft_ui_model) ----------

// A board that is weighing: Wi-Fi up, Hub online, a steady 50.120 kg on an RS232 scale.
static WsoftUiInput weighing() {
  WsoftUiInput in;
  in.wifi_up = true;
  in.rssi = -60;
  in.ip = "192.168.0.142";
  in.hub_online = true;
  in.hub_text = "HUB ONLINE";
  in.source = "RS232";
  in.linked = true;
  in.stable = true;
  in.gross = in.net = 50120;
  in.decimals = 3;
  in.unit = 1;
  return in;
}

static WsoftHiloProduct maize() {
  WsoftHiloProduct p;
  strcpy(p.code, "MZ50");
  strcpy(p.name, "Maize");
  p.target_g = 50000;
  p.low_g = 49000;
  p.high_g = 51000;
  return p;
}

static WsoftUiModel model;

void test_model_status_bar() {
  WsoftUiInput in = weighing();
  wsoft_ui_model(in, &model);
  TEST_ASSERT_TRUE(model.wifi_up);
  TEST_ASSERT_EQUAL(3, model.wifi_bars);
  TEST_ASSERT_EQUAL_STRING("192.168.0.142", model.ip);
  TEST_ASSERT_EQUAL_STRING("HUB", model.hub);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_OK, model.hub_tone);
  TEST_ASSERT_EQUAL_STRING("RS232", model.link);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_OK, model.link_tone);
  // Signal strength to bars.
  TEST_ASSERT_EQUAL(4, wsoft_ui_wifi_bars(-50));
  TEST_ASSERT_EQUAL(3, wsoft_ui_wifi_bars(-67));
  TEST_ASSERT_EQUAL(2, wsoft_ui_wifi_bars(-75));
  TEST_ASSERT_EQUAL(1, wsoft_ui_wifi_bars(-90));
  // A Hub fault shows its own words in red; not paired yet is grey, not a fault.
  in.hub_online = false;
  in.hub_text = "HUB REJECTED";
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("HUB REJECTED", model.hub);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_BAD, model.hub_tone);
  in.hub_setup = true;
  in.hub_text = "HUB SETUP";
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("HUB SETUP", model.hub);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_SOFT, model.hub_tone);
  // Wi-Fi down: no address, no bars, and the Hub is just red (the bar already says why).
  in.hub_setup = false;
  in.hub_text = "HUB NO WIFI";
  in.wifi_up = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("NO WI-FI", model.ip);
  TEST_ASSERT_EQUAL(0, model.wifi_bars);
  TEST_ASSERT_EQUAL_STRING("HUB", model.hub);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_BAD, model.hub_tone);
  // The scale link: red without a signal, yellow on an alarm.
  in.linked = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_BAD, model.link_tone);
  in.linked = true;
  in.alarm = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_WAIT, model.link_tone);
}

void test_model_the_band_above_the_weight() {
  WsoftUiInput in = weighing();
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_SLOT_NONE, model.slot);
  // A yellow save notice, in the words of wsoft_save_notice.
  in.waiting = WSOFT_SAVE_OUT_OF_LIMITS;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_SLOT_WAIT, model.slot);
  TEST_ASSERT_EQUAL_STRING("OUTSIDE HI-LO LIMITS", model.slot_title);
  TEST_ASSERT_EQUAL_STRING("Tap SAVE twice to save anyway", model.slot_detail);
  // The Wi-Fi alert goes over a yellow notice ...
  in.wifi_alert = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_SLOT_WIFI, model.slot);
  // ... and a red notice goes over the Wi-Fi alert: nothing saves until the person acts.
  in.waiting = WSOFT_SAVE_NEED_ZERO;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_SLOT_STOP, model.slot);
  TEST_ASSERT_EQUAL_STRING("TAKE THE LOAD OFF", model.slot_title);
  // "Saved, take it off" and Writer mode are not problems: no band (the SAVE box says it).
  in.wifi_alert = false;
  in.waiting = WSOFT_SAVE_ALREADY;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_SLOT_NONE, model.slot);
  in.waiting = WSOFT_SAVE_WRITER;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_SLOT_NONE, model.slot);
}

void test_model_trade_row() {
  WsoftUiInput in = weighing();
  in.audit = 7;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_FALSE(model.trade_ok);
  TEST_ASSERT_EQUAL_STRING("", model.reason);  // approval off: the star says it, no words
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_SOFT, model.reason_tone);
  TEST_ASSERT_EQUAL_STRING("AUDIT 7", model.audit);
  TEST_ASSERT_EQUAL_STRING("STABLE", model.steady);
  in.check = TRADE_OK;
  in.trade_seq = 41;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_TRUE(model.trade_ok);
  TEST_ASSERT_EQUAL_STRING("SEQ 41", model.reason);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_OK, model.reason_tone);
  in.check = TRADE_MOTION;
  in.stable = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_FALSE(model.trade_ok);
  TEST_ASSERT_EQUAL_STRING("MOTION", model.reason);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_WAIT, model.reason_tone);
  TEST_ASSERT_EQUAL_STRING("UNSTABLE", model.steady);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_WAIT, model.steady_tone);
  // The scale setup's holds win over the plain reason while Approval is on.
  in.hold = WSOFT_HOLD_BELOW_MIN;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("BELOW MIN", model.reason);
  in.hold = WSOFT_HOLD_BLANK;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("OUT OF RANGE", model.reason);
  in.check = TRADE_NO_SIGNAL;
  in.hold = WSOFT_HOLD_NONE;
  in.linked = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("NO SIGNAL", model.reason);
  TEST_ASSERT_EQUAL_STRING("NO SIGNAL", model.steady);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_BAD, model.steady_tone);
}

void test_model_weight_and_gross() {
  WsoftUiInput in = weighing();
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("NET", model.label);
  TEST_ASSERT_EQUAL_STRING("50.120", model.weight);
  TEST_ASSERT_EQUAL_STRING("kg", model.unit);
  TEST_ASSERT_EQUAL_STRING("50.120 kg", model.gross);
  TEST_ASSERT_TRUE(model.live);
  // A tare: the big number is the net weight, GROSS keeps the gross.
  in.tared = true;
  in.gross = 52620;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("NET (TARE)", model.label);
  TEST_ASSERT_EQUAL_STRING("50.120", model.weight);
  TEST_ASSERT_EQUAL_STRING("52.620 kg", model.gross);
  // Blanked: dashes for the weight, never a number that may not be shown.
  in.hold = WSOFT_HOLD_BLANK;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("----", model.weight);
  // No signal: dashes everywhere. A failed poll is never a weight.
  in.hold = WSOFT_HOLD_NONE;
  in.linked = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("----", model.weight);
  TEST_ASSERT_EQUAL_STRING("----", model.gross);
  TEST_ASSERT_FALSE(model.live);
}

void test_model_hilo_follows_the_relay_zone() {
  WsoftUiInput in = weighing();
  WsoftHiloProduct product = maize();
  in.product = &product;
  in.product_count = 2;
  in.zone = WSOFT_RELAY_OK;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_ZONE_OK, model.zone);
  TEST_ASSERT_EQUAL_STRING("OK", model.word);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_OK, model.word_tone);
  TEST_ASSERT_TRUE(model.product_on);
  TEST_ASSERT_EQUAL_STRING("MAIZE", model.product);
  TEST_ASSERT_EQUAL_STRING("TARGET 50 KG", model.target);
  TEST_ASSERT_EQUAL_STRING("49", model.low);
  TEST_ASSERT_EQUAL_STRING("51", model.high);
  TEST_ASSERT_EQUAL(wsoft_ui_pointer(50120, 49000, 51000), model.pointer);
  in.zone = WSOFT_RELAY_LOW;
  in.gross = in.net = 47350;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_ZONE_LOW, model.zone);
  TEST_ASSERT_EQUAL_STRING("TOO LOW", model.word);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_WAIT, model.word_tone);
  in.zone = WSOFT_RELAY_HIGH;
  in.gross = in.net = 52480;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_ZONE_HIGH, model.zone);
  TEST_ASSERT_EQUAL_STRING("TOO HIGH", model.word);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_BAD, model.word_tone);
  // The marker follows the net weight in the reading's own unit: 0.05 t is 50 kg.
  in.zone = WSOFT_RELAY_OK;
  in.gross = in.net = 50;
  in.decimals = 3;
  in.unit = 2;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(500, model.pointer);
}

void test_model_hilo_without_a_zone() {
  WsoftUiInput in = weighing();
  WsoftHiloProduct product = maize();
  // No product picked.
  in.product_count = 2;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_ZONE_NONE, model.zone);
  TEST_ASSERT_FALSE(model.product_on);
  TEST_ASSERT_EQUAL_STRING("NO PRODUCT", model.word);
  TEST_ASSERT_EQUAL_STRING("PICK ONE IN THE MENU", model.target);
  TEST_ASSERT_EQUAL(-1, model.pointer);
  in.product_count = 0;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("SET LIMITS ON THE HUB", model.target);
  // A product, nothing on the scale: the relays are off, so no colour and no "too low".
  in.product = &product;
  in.product_count = 2;
  in.gross = in.net = 0;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_ZONE_NONE, model.zone);
  TEST_ASSERT_EQUAL_STRING("EMPTY", model.word);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_SOFT, model.word_tone);
  TEST_ASSERT_EQUAL(0, model.pointer);
  // A load is on but the relays have not switched yet (they wait two polls): the word of
  // the check, without a colour.
  in.gross = in.net = 50120;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("OK", model.word);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_SOFT, model.word_tone);
  TEST_ASSERT_EQUAL(WSOFT_UI_ZONE_NONE, model.zone);
  // An alarm, a blanked reading and no signal each say so, and show no marker.
  in.alarm = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("ALARM", model.word);
  TEST_ASSERT_EQUAL(WSOFT_UI_TONE_BAD, model.word_tone);
  TEST_ASSERT_EQUAL(-1, model.pointer);
  in.alarm = false;
  in.hold = WSOFT_HOLD_BLANK;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("OUT OF RANGE", model.word);
  TEST_ASSERT_EQUAL(-1, model.pointer);
  in.hold = WSOFT_HOLD_NONE;
  in.linked = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("WAITING", model.word);
  TEST_ASSERT_EQUAL(-1, model.pointer);
}

void test_model_product_names() {
  WsoftUiInput in = weighing();
  WsoftHiloProduct product = maize();
  in.product = &product;
  // Capitals; a letter the screen cannot draw becomes one '?'.
  strcpy(product.name, "Caf\xC3\xA9 beans");
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("CAF? BEANS", model.product);
  // A backtick stands taller than a capital: it is drawn as an apostrophe.
  strcpy(product.name, "Mike`s mix");
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("MIKE'S MIX", model.product);
  // No name: the code.
  product.name[0] = 0;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("MZ50", model.product);
}

void test_model_save_button() {
  WsoftUiInput in = weighing();
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("TAP TO SAVE", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.save_look);
  in.auto_save = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("AUTO SAVE ON", model.save_line);
  in.pending = 3;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("3 TO SEND", model.save_line);
  // Saved: the load must come off before the next one.
  in.waiting = WSOFT_SAVE_ALREADY;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("TAKE LOAD OFF", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_BTN, model.save_look);
  // The last result shows for its 3 seconds, over everything else.
  in.recent = WSOFT_SAVE_SAVED;
  in.saved_seq = 8;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("SAVED #8", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.save_look);
  in.recent = WSOFT_SAVE_OUT_OF_LIMITS;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("TAP AGAIN: SAVE", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_BAD, model.save_look);
  in.recent = WSOFT_SAVE_NEED_ZERO;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("TAKE LOAD OFF", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_BTN, model.save_look);
  // A Writer's SAVE is saved by the weigh board: a plain TAP TO SAVE (no queue, no Auto save
  // of its own), and the weigh board's answer, SAVED without a number.
  in.recent = WSOFT_SAVE_NONE;
  in.writer = true;
  in.waiting = WSOFT_SAVE_NONE;
  in.pending = 2;
  in.auto_save = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("TAP TO SAVE", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.save_look);
  in.recent = WSOFT_SAVE_SAVED;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("SAVED", model.save_line);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.save_look);
  in.recent = WSOFT_SAVE_NOT_STABLE;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_BAD, model.save_look);
  in.pending = 0;
  in.auto_save = false;
}

void test_model_menu_tiles() {
  WsoftUiInput in = weighing();
  WsoftHiloProduct product = maize();
  in.page = WSOFT_UI_PAGE_MENU;
  in.message = "AUTO SAVE ON";
  in.auto_save = true;
  in.product_count = 0;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(WSOFT_UI_PAGE_MENU, model.page);
  TEST_ASSERT_EQUAL_STRING("MENU", model.title);
  TEST_ASSERT_EQUAL_STRING("AUTO SAVE ON", model.message);
  // An RS232 board: Approval, Auto save, Product, Screen, Wi-Fi.
  TEST_ASSERT_EQUAL(5, model.tile_count);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_APPROVAL, model.tiles[0].id);
  TEST_ASSERT_EQUAL_STRING("OFF", model.tiles[0].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_BTN, model.tiles[0].look);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_AUTO, model.tiles[1].id);
  TEST_ASSERT_EQUAL_STRING("ON", model.tiles[1].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.tiles[1].look);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_PRODUCT, model.tiles[2].id);
  TEST_ASSERT_EQUAL_STRING("NONE ON THE HUB", model.tiles[2].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_THEME, model.tiles[3].id);
  TEST_ASSERT_EQUAL_STRING("DARK", model.tiles[3].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_WIFI, model.tiles[4].id);
  TEST_ASSERT_EQUAL_STRING("192.168.0.142", model.tiles[4].value);
  // An RS485 board with a preset tare gets TARE and PRESET TARE too.
  in.rs485 = true;
  in.approval_on = true;
  in.tare_on = true;
  in.tare_value = 2500;
  in.preset_on = true;
  in.preset_text = "2.5 kg";
  in.product = &product;
  in.product_count = 2;
  in.dark = false;
  in.wifi_up = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(7, model.tile_count);
  TEST_ASSERT_EQUAL_STRING("ON", model.tiles[0].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_TARE, model.tiles[2].id);
  TEST_ASSERT_EQUAL_STRING("2.500 kg", model.tiles[2].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.tiles[2].look);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_PRESET, model.tiles[3].id);
  TEST_ASSERT_EQUAL_STRING("2.5 kg", model.tiles[3].value);
  TEST_ASSERT_EQUAL_STRING("MAIZE", model.tiles[4].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_ON, model.tiles[4].look);
  TEST_ASSERT_EQUAL_STRING("LIGHT", model.tiles[5].value);
  TEST_ASSERT_EQUAL_STRING("OFF", model.tiles[6].value);
  TEST_ASSERT_FALSE(model.dark);
  // A load cell board gets ZERO after the tare tiles. The menu holds 7: with a preset tare
  // too, WI-FI gives way (the Wi-Fi bars at the top open the same page).
  in.zero = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(7, model.tile_count);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_PRESET, model.tiles[3].id);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_ZERO, model.tiles[4].id);
  TEST_ASSERT_EQUAL_STRING("ZERO", model.tiles[4].caption);
  TEST_ASSERT_EQUAL_STRING("SET TO 0", model.tiles[4].value);
  TEST_ASSERT_EQUAL(WSOFT_UI_LOOK_BTN, model.tiles[4].look);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_PRODUCT, model.tiles[5].id);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_THEME, model.tiles[6].id);
  // Without a preset tare everything fits, WI-FI last.
  in.preset_on = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(7, model.tile_count);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_ZERO, model.tiles[3].id);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_WIFI, model.tiles[6].id);
  // On the live page ZERO takes the place of GROSS, and says what the last tap did.
  in.page = WSOFT_UI_PAGE_LIVE;
  in.tared = false;
  in.source = "LOADCELL";
  wsoft_ui_model(in, &model);
  TEST_ASSERT_TRUE(model.zero_btn);
  TEST_ASSERT_EQUAL_STRING("TAP: SET TO 0", model.zero_line);
  TEST_ASSERT_EQUAL_STRING("CELL", model.link);  // the bar holds 7 letters
  in.zero_note = "NOT STEADY";
  uint32_t before = wsoft_ui_row_key(model, WSOFT_UI_ROW_BOTTOM);
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("NOT STEADY", model.zero_line);
  TEST_ASSERT_NOT_EQUAL(before, wsoft_ui_row_key(model, WSOFT_UI_ROW_BOTTOM));  // the row is painted again
  // Under a tare the gross is shown again (a zero is refused then anyway).
  in.tared = true;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_FALSE(model.zero_btn);
  TEST_ASSERT_EQUAL_STRING("", model.zero_line);
  TEST_ASSERT_TRUE(WSOFT_UI_BOX_ZERO.hit(60, 213));
  TEST_ASSERT_FALSE(WSOFT_UI_BOX_ZERO.hit(118, 213));  // the gap before MENU
  in.page = WSOFT_UI_PAGE_MENU;
  in.source = "RS485";
  in.zero_note = "";
  // No ZERO on a board that reads a scale (RS485, RS232): that scale keeps its own zero.
  in.zero = false;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL(6, model.tile_count);
  for (int i = 0; i < model.tile_count; i++) TEST_ASSERT_NOT_EQUAL(WSOFT_UI_TILE_ZERO, model.tiles[i].id);
}

void test_the_model_as_json_for_the_browser_copy() {
  static char json[4096];
  WsoftUiModel model;
  // Every test screen fits and is one whole object.
  for (int n = 0; n < WSOFT_UI_PRESET_COUNT; n++) {
    TEST_ASSERT_TRUE(wsoft_ui_preset(n, &model));
    size_t length = wsoft_ui_model_json(model, json, sizeof json);
    TEST_ASSERT_TRUE(length > 0);
    TEST_ASSERT_EQUAL(length, strlen(json));
    TEST_ASSERT_EQUAL('{', json[0]);
    TEST_ASSERT_EQUAL('}', json[length - 1]);
  }
  // The words and states, under the model's own field names.
  WsoftUiInput in = weighing();
  in.zero = true;
  in.source = "LOADCELL";
  wsoft_ui_model(in, &model);
  TEST_ASSERT_TRUE(wsoft_ui_model_json(model, json, sizeof json) > 0);
  TEST_ASSERT_NOT_NULL(strstr(json, "{\"dark\":true,\"page\":0,\"style\":0,"));
  in.style = WSOFT_UI_STYLE_DIAL;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_TRUE(wsoft_ui_model_json(model, json, sizeof json) > 0);
  TEST_ASSERT_NOT_NULL(strstr(json, "{\"dark\":true,\"page\":0,\"style\":2,"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"link\":\"CELL\""));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"zero_btn\":true,\"zero_line\":\"TAP: SET TO 0\""));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"pointer\":"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"tiles\":[{\"caption\":\"APPROVAL\",\"value\":\"OFF\",\"look\":0},"));
  TEST_ASSERT_NOT_NULL(strstr(json, "],\"pin_count\":0,\"address\":"));
  // A quote or a backslash in a word (a product name from the Hub) cannot break the object.
  strcpy(model.product, "5\" PIPE \\ A");
  model.message[0] = '\n';
  model.message[1] = 0;
  TEST_ASSERT_TRUE(wsoft_ui_model_json(model, json, sizeof json) > 0);
  TEST_ASSERT_NOT_NULL(strstr(json, "\"product\":\"5\\\" PIPE \\\\ A\""));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"message\":\" \""));
  // The fullest model there can be still fits WSOFT_UI_JSON_MAX bytes: every text full of quotes (each
  // takes two bytes), every tile there.
  memset(&model, 0, sizeof model);
#define FILL(field) memset(field, '"', sizeof(field) - 1)
  FILL(model.ip); FILL(model.hub); FILL(model.link); FILL(model.slot_title); FILL(model.slot_detail);
  FILL(model.reason); FILL(model.steady); FILL(model.plate_sa); FILL(model.plate_limits); FILL(model.audit);
  FILL(model.label); FILL(model.weight); FILL(model.unit); FILL(model.word); FILL(model.product);
  FILL(model.target); FILL(model.low); FILL(model.high); FILL(model.gross); FILL(model.save_line);
  FILL(model.zero_line); FILL(model.title); FILL(model.message); FILL(model.address); FILL(model.ap_name);
  model.tile_count = WSOFT_UI_TILES_MAX;
  for (int i = 0; i < WSOFT_UI_TILES_MAX; i++) {
    FILL(model.tiles[i].caption);
    FILL(model.tiles[i].value);
    model.tiles[i].look = 255;
  }
#undef FILL
  model.pointer = -32768;
  model.page = model.wifi_bars = model.hub_tone = model.link_tone = model.slot = model.zone = model.pin_count = 255;
  size_t fullest = wsoft_ui_model_json(model, json, sizeof json);
  TEST_ASSERT_TRUE(fullest > 0);
  TEST_ASSERT_TRUE(fullest < WSOFT_UI_JSON_MAX);
  // Too small a buffer gives nothing, never half an object.
  static char small[64];
  small[0] = 'x';
  TEST_ASSERT_EQUAL(0, (int)wsoft_ui_model_json(model, small, sizeof small));
  TEST_ASSERT_EQUAL(0, small[0]);
  TEST_ASSERT_EQUAL(0, (int)wsoft_ui_model_json(model, json, fullest));  // one byte short
  TEST_ASSERT_TRUE(wsoft_ui_model_json(model, json, fullest + 1) > 0);
}

void test_model_pages() {
  WsoftUiInput in = weighing();
  in.page = WSOFT_UI_PAGE_PIN;
  in.pin_count = 3;
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("ENTER PIN", model.title);
  TEST_ASSERT_EQUAL(3, model.pin_count);
  in.page = WSOFT_UI_PAGE_NO_PIN;
  in.address = "192.168.0.142";
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("APPROVAL", model.title);
  TEST_ASSERT_EQUAL_STRING("192.168.0.142", model.address);
  in.page = WSOFT_UI_PAGE_WIFI;
  in.wifi_up = false;
  in.address = "192.168.4.1";
  in.ap_name = "Weighsoft-Esp-NEW";
  wsoft_ui_model(in, &model);
  TEST_ASSERT_EQUAL_STRING("WI-FI", model.title);
  TEST_ASSERT_EQUAL_STRING("Weighsoft-Esp-NEW", model.ap_name);
}

void test_model_same_facts_same_bytes() {
  // The board compares models byte by byte to see what changed.
  WsoftUiInput in = weighing();
  static WsoftUiModel a, b;
  memset(&a, 0xFF, sizeof a);
  memset(&b, 0x00, sizeof b);
  wsoft_ui_model(in, &a);
  wsoft_ui_model(in, &b);
  TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);
  // And a text too long for its place is cut, never run over its neighbour.
  in.ip = "1234:5678:9abc:def0:1234:5678:9abc:def0";
  in.message = "A MESSAGE THAT IS MUCH TOO LONG FOR THE ROOM IT HAS ON THE SCREEN";
  wsoft_ui_model(in, &a);
  TEST_ASSERT_EQUAL(sizeof a.ip - 1, strlen(a.ip));
  TEST_ASSERT_EQUAL(sizeof a.message - 1, strlen(a.message));
}

void test_only_rows_that_changed_get_painted() {
  // The board paints a row again when its key changes. A row's key covers what it draws.
  WsoftUiInput in = weighing();
  WsoftHiloProduct product = maize();
  in.product = &product;
  in.product_count = 1;
  in.zone = WSOFT_RELAY_OK;
  static WsoftUiModel a, b;
  wsoft_ui_model(in, &a);
  wsoft_ui_model(in, &b);
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, row), wsoft_ui_row_key(b, row));
  }
  // The weight moves 10 g: the weight, the marker and GROSS change; the bar and trade row do not.
  in.gross = in.net = 50130;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_BAR), wsoft_ui_row_key(b, WSOFT_UI_ROW_BAR));
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_TOP), wsoft_ui_row_key(b, WSOFT_UI_ROW_TOP));
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_WEIGHT), wsoft_ui_row_key(b, WSOFT_UI_ROW_WEIGHT));
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_HILO), wsoft_ui_row_key(b, WSOFT_UI_ROW_HILO));
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_BOTTOM), wsoft_ui_row_key(b, WSOFT_UI_ROW_BOTTOM));
  // Only the steady word changes: only the trade row.
  in.gross = in.net = 50120;
  in.stable = false;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_TOP), wsoft_ui_row_key(b, WSOFT_UI_ROW_TOP));
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_WEIGHT), wsoft_ui_row_key(b, WSOFT_UI_ROW_WEIGHT));
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_BOTTOM), wsoft_ui_row_key(b, WSOFT_UI_ROW_BOTTOM));
  // The address changes: the status bar, and the pages that show it. Not the weight.
  in.stable = true;
  in.ip = "192.168.0.143";
  wsoft_ui_model(in, &b);
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_BAR), wsoft_ui_row_key(b, WSOFT_UI_ROW_BAR));
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_PAGE), wsoft_ui_row_key(b, WSOFT_UI_ROW_PAGE));
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_WEIGHT), wsoft_ui_row_key(b, WSOFT_UI_ROW_WEIGHT));
  // The signal flickers by a bar: the status bar only. A menu page must not be painted again
  // four times a second because of it.
  in.ip = "192.168.0.142";
  wsoft_ui_model(in, &a);
  in.rssi = -80;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_NOT_EQUAL(a.wifi_bars, b.wifi_bars);
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_BAR), wsoft_ui_row_key(b, WSOFT_UI_ROW_BAR));
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_PAGE), wsoft_ui_row_key(b, WSOFT_UI_ROW_PAGE));
  in.rssi = -60;
  // Wi-Fi drops: the pages that show the address follow.
  in.wifi_up = false;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_PAGE), wsoft_ui_row_key(b, WSOFT_UI_ROW_PAGE));
  in.wifi_up = true;
  // GROSS is greyed without a signal, so the bottom row follows `live` too.
  in.ip = "192.168.0.142";
  wsoft_ui_model(in, &a);
  a.live = true;
  b = a;
  b.live = false;
  TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, WSOFT_UI_ROW_BOTTOM), wsoft_ui_row_key(b, WSOFT_UI_ROW_BOTTOM));
  // Light or dark changes every row.
  b = a;
  b.dark = !a.dark;
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, row), wsoft_ui_row_key(b, row));
  }
}

void test_the_style_of_the_live_page() {
  WsoftUiInput in = weighing();
  WsoftUiModel a, b;
  wsoft_ui_model(in, &a);
  TEST_ASSERT_EQUAL(WSOFT_UI_STYLE_CLASSIC, a.style);
  // A number that is no style (a worn flash cell) is the classic screen.
  in.style = WSOFT_UI_STYLE_COUNT;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_EQUAL(WSOFT_UI_STYLE_CLASSIC, b.style);
  in.style = -1;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_EQUAL(WSOFT_UI_STYLE_CLASSIC, b.style);
  // The SCREEN tile names the look that is on.
  in.page = WSOFT_UI_PAGE_MENU;
  in.style = WSOFT_UI_STYLE_COLOUR;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_EQUAL(WSOFT_UI_TILE_THEME, b.tiles[3].id);
  TEST_ASSERT_EQUAL_STRING("COLOUR", b.tiles[3].value);
  in.style = WSOFT_UI_STYLE_DIAL;
  wsoft_ui_model(in, &b);
  TEST_ASSERT_EQUAL_STRING("DIAL", b.tiles[3].value);
  in.page = WSOFT_UI_PAGE_LIVE;
  // Another style paints every live row again.
  for (int style = WSOFT_UI_STYLE_COLOUR; style < WSOFT_UI_STYLE_COUNT; style++) {
    in.style = style;
    wsoft_ui_model(in, &b);
    TEST_ASSERT_EQUAL(style, b.style);
    for (int row = 0; row < WSOFT_UI_ROW_PAGE; row++) {
      TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(a, row), wsoft_ui_row_key(b, row));
    }
    // The zone colours the whole Full Colour screen, and the dial's arc and bar: every live
    // row follows it (on the classic screen the status bar and the bottom row do not).
    WsoftUiModel z = b;
    z.zone = (uint8_t)(b.zone == WSOFT_UI_ZONE_HIGH ? WSOFT_UI_ZONE_LOW : WSOFT_UI_ZONE_HIGH);
    TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_TOP), wsoft_ui_row_key(z, WSOFT_UI_ROW_TOP));
    TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_WEIGHT), wsoft_ui_row_key(z, WSOFT_UI_ROW_WEIGHT));
    TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_HILO), wsoft_ui_row_key(z, WSOFT_UI_ROW_HILO));
    if (style == WSOFT_UI_STYLE_COLOUR) {
      TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_BAR), wsoft_ui_row_key(z, WSOFT_UI_ROW_BAR));
      TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_BOTTOM), wsoft_ui_row_key(z, WSOFT_UI_ROW_BOTTOM));
    }
    // The dial's needle is drawn over the top and weight rows: they follow the marker.
    if (style == WSOFT_UI_STYLE_DIAL) {
      z = b;
      z.pointer = (int16_t)(b.pointer > 500 ? 100 : 900);
      TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_TOP), wsoft_ui_row_key(z, WSOFT_UI_ROW_TOP));
      TEST_ASSERT_NOT_EQUAL(wsoft_ui_row_key(b, WSOFT_UI_ROW_WEIGHT), wsoft_ui_row_key(z, WSOFT_UI_ROW_WEIGHT));
    }
  }
  a.zone = WSOFT_UI_ZONE_OK;
  WsoftUiModel c = a;
  c.zone = WSOFT_UI_ZONE_HIGH;
  TEST_ASSERT_EQUAL_HEX32(wsoft_ui_row_key(a, WSOFT_UI_ROW_BAR), wsoft_ui_row_key(c, WSOFT_UI_ROW_BAR));
}

void test_what_to_do_next_to_the_hilo_word() {
  WsoftUiInput in = weighing();
  WsoftUiModel m;
  // Too low: add more. Too high: remove product. In every style.
  in.zone = WSOFT_RELAY_LOW;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("TOO LOW", m.word);
  TEST_ASSERT_EQUAL_STRING("ADD MORE", m.hint);
  in.zone = WSOFT_RELAY_HIGH;
  in.style = WSOFT_UI_STYLE_DIAL;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("TOO HIGH", m.word);
  TEST_ASSERT_EQUAL_STRING("REMOVE PRODUCT", m.hint);
  // Inside the limits the classic screen keeps OK; the new screens say what the save does.
  in.zone = WSOFT_RELAY_OK;
  in.auto_save = true;
  in.waiting = WSOFT_SAVE_NONE;
  in.recent = WSOFT_SAVE_NONE;
  in.style = WSOFT_UI_STYLE_CLASSIC;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("OK", m.word);
  in.style = WSOFT_UI_STYLE_COLOUR;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("SAVING", m.word);
  TEST_ASSERT_EQUAL_STRING("HOLD STILL", m.hint);
  // Auto save counts down: on the word and on the SAVE button.
  in.countdown = 3;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("SAVING", m.word);
  TEST_ASSERT_EQUAL_STRING("IN 3", m.hint);
  TEST_ASSERT_EQUAL_STRING("SAVING IN 3", m.save_line);
  in.style = WSOFT_UI_STYLE_CLASSIC;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("OK", m.word);
  TEST_ASSERT_EQUAL_STRING("SAVING IN 3", m.save_line);
  in.style = WSOFT_UI_STYLE_COLOUR;
  in.countdown = 0;
  // Saved, and while the saved load is still on.
  in.recent = WSOFT_SAVE_SAVED;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("DONE", m.word);
  TEST_ASSERT_EQUAL_STRING("TAKE IT OFF", m.hint);
  in.recent = WSOFT_SAVE_NONE;
  in.waiting = WSOFT_SAVE_ALREADY;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("DONE", m.word);
  // Auto save off, or a Writer: a tap saves.
  in.waiting = WSOFT_SAVE_NONE;
  in.auto_save = false;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("GOOD", m.word);
  TEST_ASSERT_EQUAL_STRING("TAP SAVE", m.hint);
  // Something stops the save: never SAVING. The notice band says what.
  in.auto_save = true;
  in.waiting = WSOFT_SAVE_NEED_ZERO;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("GOOD", m.word);
  TEST_ASSERT_EQUAL_STRING("", m.hint);
  // No zone, no hint.
  in.zone = WSOFT_RELAY_NONE;
  in.waiting = WSOFT_SAVE_NONE;
  wsoft_ui_model(in, &m);
  TEST_ASSERT_EQUAL_STRING("", m.hint);
}

void test_model_plate_line() {
  WsoftScaleSetup setup;
  WsoftCardFormat card;
  char sa[40], limits[56];
  // Nothing set: no plate.
  wsoft_ui_plate_text(setup, card, sa, sizeof sa, limits, sizeof limits);
  TEST_ASSERT_EQUAL_STRING("", sa);
  TEST_ASSERT_EQUAL_STRING("", limits);
  TEST_ASSERT_TRUE(wsoft_kg_parse("150", &setup.max_kg));
  TEST_ASSERT_TRUE(wsoft_kg_parse("1", &setup.min_kg));
  strcpy(setup.sa_number, "1234/56");
  card.read = true;
  card.division = 5;
  card.decimal = 2;
  card.unit = 1;
  wsoft_ui_plate_text(setup, card, sa, sizeof sa, limits, sizeof limits);
  TEST_ASSERT_EQUAL_STRING("SA 1234/56", sa);
  TEST_ASSERT_EQUAL_STRING("MAX 150 MIN 1 E 0.05 KG", limits);
  // What is not known yet shows as '-' or '?', as on the old screen.
  setup.sa_number[0] = 0;
  setup.min_kg = WSOFT_KG_UNSET;
  card.read = false;
  wsoft_ui_plate_text(setup, card, sa, sizeof sa, limits, sizeof limits);
  TEST_ASSERT_EQUAL_STRING("SA -", sa);
  TEST_ASSERT_EQUAL_STRING("MAX 150 MIN - E ? KG", limits);
  // The longest SA number the setup takes still fits its part.
  memset(setup.sa_number, 'A', WSOFT_SA_NUMBER_MAX);
  setup.sa_number[WSOFT_SA_NUMBER_MAX] = 0;
  wsoft_ui_plate_text(setup, card, sa, sizeof sa, limits, sizeof limits);
  TEST_ASSERT_EQUAL(3 + WSOFT_SA_NUMBER_MAX, strlen(sa));
}

void test_a_plate_never_shows_a_cut_number() {
  // A cut limit would be a wrong limit ("MIN 4" for "MIN 400"): the screen cuts the SA
  // number instead, and marks it.
  static uint16_t frame[320 * 16];
  WsoftCanvas c;
  c.begin(320, 240, 1, 1, WSOFT_UI_FONTS_1X);
  c.window(frame, 0, 16);
  static WsoftUiModel m;
  char line[128];
  memset(&m, 0, sizeof m);
  // It all fits: the line as it always was.
  strcpy(m.plate_sa, "SA 1234/56");
  strcpy(m.plate_limits, "MAX 150 MIN 1 E 0.05 KG");
  TEST_ASSERT_EQUAL_STRING("SA 1234/56   MAX 150 MIN 1 E 0.05 KG", wsoft_ui_plate_line(c, m, 250, line, sizeof line));
  // Too long: the limits stay whole, the SA number ends in dots.
  strcpy(m.plate_sa, "SA NRCS/SA1234/2026-0001234567890");
  strcpy(m.plate_limits, "MAX 60000 MIN 400 E 20 KG");
  const char *cut = wsoft_ui_plate_line(c, m, 230, line, sizeof line);
  TEST_ASSERT_TRUE(c.width(cut, WSOFT_UI_TINY) <= 230);
  const char *tail = strstr(cut, "...   MAX 60000 MIN 400 E 20 KG");
  TEST_ASSERT_NOT_NULL(tail);
  TEST_ASSERT_EQUAL_STRING("...   MAX 60000 MIN 400 E 20 KG", tail);
  TEST_ASSERT_EQUAL_STRING_LEN("SA NRCS", cut, 7);
  // No room for any of the SA number: the limits alone.
  TEST_ASSERT_EQUAL_STRING("MAX 60000 MIN 400 E 20 KG", wsoft_ui_plate_line(c, m, 170, line, sizeof line));
  // Not even the limits fit: whole words only, and dots say something is missing.
  cut = wsoft_ui_plate_line(c, m, 100, line, sizeof line);
  TEST_ASSERT_TRUE(c.width(cut, WSOFT_UI_TINY) <= 100);
  size_t kept = strlen(cut) - 4;
  TEST_ASSERT_EQUAL_STRING(" ...", cut + kept);
  TEST_ASSERT_EQUAL_STRING_LEN(m.plate_limits, cut, kept);
  TEST_ASSERT_EQUAL(' ', m.plate_limits[kept]);  // cut between two words, never inside a number
  // No plate: nothing.
  m.plate_sa[0] = m.plate_limits[0] = 0;
  TEST_ASSERT_EQUAL_STRING("", wsoft_ui_plate_line(c, m, 250, line, sizeof line));
}

// ---------- every test screen against the reference drawing ----------

static uint16_t frame[800 * 480];
static uint16_t band[800 * 32];

static void save_failed(const char *panel, int n, int count) {
  char name[64];
  snprintf(name, sizeof name, ".pio/wsoft-ui-fail-%s-%02d.bin", panel, n);
  FILE *f = fopen(name, "wb");
  if (f == nullptr) return;
  fwrite(frame, sizeof frame[0], count, f);
  fclose(f);
  printf("screen %d on the %s panel differs: wrote %s\n", n, panel, name);
}

static void check_panel(const char *panel, int width, int height, double sx, double sy, const WsoftFont *fonts,
                        const uint32_t *golden) {
  WsoftCanvas c;
  c.begin(width, height, sx, sy, fonts);
  static WsoftUiModel model;
  char message[64];
  for (int n = 0; n < WSOFT_UI_PRESET_COUNT; n++) {
    TEST_ASSERT_TRUE(wsoft_ui_preset(n, &model));
    // The whole frame at once (the Waveshare's way).
    memset(frame, 0x55, sizeof frame);
    c.window(frame, 0, height);
    wsoft_ui_paint(c, model);
    uint32_t whole = wsoft_crc32(0, (const uint8_t *)frame, (size_t)width * height * 2);
    if (whole != golden[n]) save_failed(panel, n, width * height);
    snprintf(message, sizeof message, "screen %d, %s panel, whole frame", n, panel);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(golden[n], whole, message);
    // Row by row in bands (the yellow board's way): each band starts as rubbish.
    uint32_t banded = 0;
    int rows_per_band = (int)(16 * sy);
    for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
      if (!wsoft_ui_row_on_page(row, model.page)) continue;
      WsoftUiSpan span = wsoft_ui_row_span(row);
      int y0 = (int)(span.y0 * sy), y1 = (int)(span.y1 * sy);
      for (int top = y0; top < y1; top += rows_per_band) {
        int rows = y1 - top < rows_per_band ? y1 - top : rows_per_band;
        memset(band, 0xAA, sizeof band);
        c.window(band, top, rows);
        wsoft_ui_paint_row(c, model, row);
        banded = wsoft_crc32(banded, (const uint8_t *)band, (size_t)width * rows * 2);
      }
    }
    snprintf(message, sizeof message, "screen %d, %s panel, in bands", n, panel);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(golden[n], banded, message);
  }
}

void test_every_screen_on_the_yellow_board() {
  check_panel("yellow", 320, 240, 1, 1, WSOFT_UI_FONTS_1X, GOLDEN_1X);
}

void test_every_screen_on_the_waveshare() {
  check_panel("wave", 800, 480, 2.5, 2, WSOFT_UI_FONTS_2X, GOLDEN_2X);
}

void test_there_is_a_golden_for_every_screen() {
  TEST_ASSERT_EQUAL(WSOFT_UI_PRESET_COUNT, (int)(sizeof GOLDEN_1X / sizeof GOLDEN_1X[0]));
  TEST_ASSERT_EQUAL(WSOFT_UI_PRESET_COUNT, (int)(sizeof GOLDEN_2X / sizeof GOLDEN_2X[0]));
  static WsoftUiModel model;
  TEST_ASSERT_FALSE(wsoft_ui_preset(WSOFT_UI_PRESET_COUNT, &model));
  TEST_ASSERT_FALSE(wsoft_ui_preset(-1, &model));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_blend_rounds_each_channel);
  RUN_TEST(test_crc32_is_the_zip_one);
  RUN_TEST(test_a_rect_stays_inside_its_window);
  RUN_TEST(test_a_rect_scales_with_the_panel);
  RUN_TEST(test_round_corners_cut_the_corner_dots);
  RUN_TEST(test_a_ring_paints_the_same_as_looking_at_every_dot);
  RUN_TEST(test_text_width_and_alignment);
  RUN_TEST(test_marker_position_on_the_bar);
  RUN_TEST(test_tap_boxes);
  RUN_TEST(test_model_status_bar);
  RUN_TEST(test_model_the_band_above_the_weight);
  RUN_TEST(test_model_trade_row);
  RUN_TEST(test_model_weight_and_gross);
  RUN_TEST(test_model_hilo_follows_the_relay_zone);
  RUN_TEST(test_model_hilo_without_a_zone);
  RUN_TEST(test_model_product_names);
  RUN_TEST(test_model_save_button);
  RUN_TEST(test_model_menu_tiles);
  RUN_TEST(test_the_model_as_json_for_the_browser_copy);
  RUN_TEST(test_model_pages);
  RUN_TEST(test_model_same_facts_same_bytes);
  RUN_TEST(test_only_rows_that_changed_get_painted);
  RUN_TEST(test_the_style_of_the_live_page);
  RUN_TEST(test_what_to_do_next_to_the_hilo_word);
  RUN_TEST(test_model_plate_line);
  RUN_TEST(test_a_plate_never_shows_a_cut_number);
  RUN_TEST(test_there_is_a_golden_for_every_screen);
  RUN_TEST(test_every_screen_on_the_yellow_board);
  RUN_TEST(test_every_screen_on_the_waveshare);
  return UNITY_END();
}
