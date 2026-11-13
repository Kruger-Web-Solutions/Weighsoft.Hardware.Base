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
  RUN_TEST(test_text_width_and_alignment);
  RUN_TEST(test_marker_position_on_the_bar);
  RUN_TEST(test_tap_boxes);
  RUN_TEST(test_there_is_a_golden_for_every_screen);
  RUN_TEST(test_every_screen_on_the_yellow_board);
  RUN_TEST(test_every_screen_on_the_waveshare);
  return UNITY_END();
}
