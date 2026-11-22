#pragma once

// The W-Soft drawing engine: boxes with round corners, discs and letters into a buffer of
// 16-bit dots. Pure C++ (no Arduino), so the host tests draw the same dots as the boards
// (test/test_wsoft_ui). The reference it must match dot for dot is tools/wsoft-ui/ref.js.
//
// Everything is placed in layout units, 320 x 240. A panel scales them: the yellow board
// 1 x 1, the Waveshare 2.5 x 2. The canvas draws into a window of panel lines, so the same
// calls fill a whole frame (Waveshare, in PSRAM) or one band of lines at a time (yellow
// board, no memory for a frame). Whatever falls outside the window is skipped.
//
// The maths is in double on purpose: it is the reference drawing's own maths, so a corner or
// a letter lands on the same dot on a PC, an ESP32 and an ESP32-S3.

#include <stddef.h>
#include <stdint.h>

// One letter: where its coverage rows start, its size, where it sits from the pen and the
// baseline (dy is negative above the baseline), and how far the pen moves.
struct WsoftGlyph {
  uint32_t offset;
  uint8_t w, h;
  int8_t dx, dy;
  uint8_t adv;
};

// A set of letters at one size: `count` characters from `first`. alpha holds 4-bit coverage,
// two dots a byte, each row starting on a byte. cap is the height of a capital; track is the
// letter spacing already added to every adv.
struct WsoftFont {
  const WsoftGlyph *glyphs;
  const uint8_t *alpha;
  uint8_t first, count, cap;
  int8_t track;
};

enum WsoftAlign { WSOFT_LEFT, WSOFT_CENTER, WSOFT_RIGHT };

// fg over bg with alpha 0..255, each of the 5-6-5 channels rounded to nearest.
uint16_t wsoft_blend565(uint16_t bg, uint16_t fg, int alpha);

// CRC-32 as in zip. Pass the result back in to carry on over a second block; start with 0.
uint32_t wsoft_crc32(uint32_t crc, const uint8_t *data, size_t length);

class WsoftCanvas {
 public:
  // A panel of width x height dots; (sx, sy) scale layout units to dots. fonts is indexed by
  // the font numbers given to text().
  void begin(int width, int height, double sx, double sy, const WsoftFont *fonts);
  // Draw into pixels: `rows` panel lines of `width` dots, the first being panel line `top`.
  void window(uint16_t *pixels, int top, int rows);
  // Only the layout lines y0 up to y1 are drawn from here on, whatever the window: a row of the
  // screen never paints outside itself. begin() opens it to the whole panel.
  void clip(double y0, double y1);

  // r: corner radius in layout units; below 0 rounds the short side fully (a pill).
  void rect(double x, double y, double w, double h, uint16_t color, double r = 0);
  void disc(double cx, double cy, double r, uint16_t color);
  // A line with round ends, w thick (the tick and the arrows of the Hi-Lo word).
  void line(double x1, double y1, double x2, double y2, double w, uint16_t color);
  // A piece of a ring: radius r, w thick inwards, from the direction (ax, ay) clockwise to
  // (bx, by), less than half a turn. Directions are unit lengths, y down. A circle on every
  // panel: the radius scales with the height, like disc().
  void ring(double cx, double cy, double r, double w, double ax, double ay, double bx, double by, uint16_t color);
  // A six-pointed star (two triangles), r = centre to a point. Edges by 4 x 4 samples a dot.
  void star6(double cx, double cy, double r, uint16_t color);

  // Width of text in panel dots. Characters the font does not hold take no room.
  int dots(const char *text, int font) const;
  // Width in layout units.
  double width(const char *text, int font) const;
  // Copies into out the longest start of text that is no wider than max layout units (and
  // fits size). Returns out.
  const char *fit(const char *text, int font, double max, char *out, size_t size) const;
  // y is the middle of the capital letters. Returns the width in layout units.
  double text(const char *text, double x, double y, int font, uint16_t color, WsoftAlign align = WSOFT_LEFT);

  int panel_width() const { return width_; }
  int panel_height() const { return height_; }
  double scale_x() const { return sx_; }
  double scale_y() const { return sy_; }

 private:
  void put(int px, int py, uint16_t color, int alpha) {
    uint16_t &dot = pixels_[(py - top_) * width_ + px];
    dot = alpha >= 255 ? color : wsoft_blend565(dot, color, alpha);
  }

  // The first panel line that may be drawn and the one after the last: the window and the clip.
  long lo() const { return top_ > clip0_ ? top_ : clip0_; }
  long hi() const { return top_ + rows_ < clip1_ ? top_ + rows_ : clip1_; }

  uint16_t *pixels_ = nullptr;
  int width_ = 0, height_ = 0, top_ = 0, rows_ = 0;
  long clip0_ = 0, clip1_ = 0;
  double sx_ = 1, sy_ = 1;
  const WsoftFont *fonts_ = nullptr;
};
