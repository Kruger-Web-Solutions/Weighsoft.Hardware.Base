#include "wsoft_gfx.h"

#include <math.h>

// Line-for-line with class Canvas in tools/wsoft-ui/ref.js. Keep the order of every sum and
// product the same there and here, or a dot lands differently and test_wsoft_ui fails.

static long rnd(double v) {
  return (long)floor(v + 0.5);
}

uint16_t wsoft_blend565(uint16_t bg, uint16_t fg, int alpha) {
  if (alpha >= 255) return fg;
  int ia = 255 - alpha;
  int r = (((fg >> 11) & 31) * alpha + ((bg >> 11) & 31) * ia + 127) / 255;
  int g = (((fg >> 5) & 63) * alpha + ((bg >> 5) & 63) * ia + 127) / 255;
  int b = ((fg & 31) * alpha + (bg & 31) * ia + 127) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

uint32_t wsoft_crc32(uint32_t crc, const uint8_t *data, size_t length) {
  crc = ~crc;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
  }
  return ~crc;
}

void WsoftCanvas::begin(int width, int height, double sx, double sy, const WsoftFont *fonts) {
  width_ = width;
  height_ = height;
  sx_ = sx;
  sy_ = sy;
  fonts_ = fonts;
  pixels_ = nullptr;
  top_ = rows_ = 0;
}

void WsoftCanvas::window(uint16_t *pixels, int top, int rows) {
  pixels_ = pixels;
  top_ = top;
  rows_ = rows;
}

void WsoftCanvas::rect(double x, double y, double w, double h, uint16_t color, double r) {
  long X0 = rnd(x * sx_), Y0 = rnd(y * sy_), X1 = rnd((x + w) * sx_), Y1 = rnd((y + h) * sy_);
  double R = r < 0 ? (X1 - X0 < Y1 - Y0 ? X1 - X0 : Y1 - Y0) / 2.0 : r * sy_;
  if (R > (X1 - X0) / 2.0) R = (X1 - X0) / 2.0;
  if (R > (Y1 - Y0) / 2.0) R = (Y1 - Y0) / 2.0;
  long first = Y0 > top_ ? Y0 : top_, last = Y1 < top_ + rows_ ? Y1 : top_ + rows_;
  long left = X0 > 0 ? X0 : 0, right = X1 < width_ ? X1 : width_;
  for (long py = first; py < last; py++) {
    bool corner_row = false;
    double cy = 0;
    if (py + 0.5 < Y0 + R) {
      corner_row = true;
      cy = Y0 + R;
    } else if (py + 0.5 > Y1 - R) {
      corner_row = true;
      cy = Y1 - R;
    }
    uint16_t *line = pixels_ + (py - top_) * width_;
    if (!corner_row) {
      for (long px = left; px < right; px++) line[px] = color;
      continue;
    }
    for (long px = left; px < right; px++) {
      int alpha = 255;
      bool corner = false;
      double cx = 0;
      if (px + 0.5 < X0 + R) {
        corner = true;
        cx = X0 + R;
      } else if (px + 0.5 > X1 - R) {
        corner = true;
        cx = X1 - R;
      }
      if (corner) {
        double dx = px + 0.5 - cx, dy = py + 0.5 - cy;
        double cover = R - sqrt(dx * dx + dy * dy) + 0.5;
        if (cover <= 0) continue;
        if (cover < 1) alpha = (int)rnd(cover * 255);
      }
      line[px] = alpha >= 255 ? color : wsoft_blend565(line[px], color, alpha);
    }
  }
}

void WsoftCanvas::disc(double cx, double cy, double r, uint16_t color) {
  double X = cx * sx_, Y = cy * sy_, R = r * sy_;
  long first = (long)floor(Y - R - 1), last = (long)floor(Y + R + 1);
  long left = (long)floor(X - R - 1), right = (long)floor(X + R + 1);
  if (first < top_) first = top_;
  if (last > top_ + rows_ - 1) last = top_ + rows_ - 1;
  if (left < 0) left = 0;
  if (right > width_ - 1) right = width_ - 1;
  for (long py = first; py <= last; py++) {
    for (long px = left; px <= right; px++) {
      double dx = px + 0.5 - X, dy = py + 0.5 - Y;
      double cover = R - sqrt(dx * dx + dy * dy) + 0.5;
      if (cover <= 0) continue;
      put((int)px, (int)py, color, cover < 1 ? (int)rnd(cover * 255) : 255);
    }
  }
}


// Inside the six-pointed star of radius R at the origin: either triangle.
static bool in_star6(double x, double y, double R) {
  double k = sqrt(3.0) * fabs(x);
  return (y <= R / 2 && y + R >= k) || (y >= -R / 2 && R - y >= k);
}

void WsoftCanvas::star6(double cx, double cy, double r, uint16_t color) {
  double X = cx * sx_, Y = cy * sy_, R = r * sy_;
  long first = (long)floor(Y - R - 1), last = (long)floor(Y + R + 1);
  long left = (long)floor(X - R - 1), right = (long)floor(X + R + 1);
  if (first < top_) first = top_;
  if (last > top_ + rows_ - 1) last = top_ + rows_ - 1;
  if (left < 0) left = 0;
  if (right > width_ - 1) right = width_ - 1;
  for (long py = first; py <= last; py++) {
    for (long px = left; px <= right; px++) {
      int n = 0;
      for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
          if (in_star6(px + (i + 0.5) / 4 - X, py + (j + 0.5) / 4 - Y, R)) n++;
        }
      }
      if (n == 0) continue;
      put((int)px, (int)py, color, n == 16 ? 255 : (int)rnd(n * 255.0 / 16));
    }
  }
}

int WsoftCanvas::dots(const char *text, int font) const {
  const WsoftFont &f = fonts_[font];
  int w = 0;
  for (const char *p = text; *p; p++) {
    int k = (uint8_t)*p - f.first;
    if (k >= 0 && k < f.count) w += f.glyphs[k].adv;
  }
  // The last letter's spacing is not part of the text.
  return w > 0 && w > f.track ? w - f.track : 0;
}

double WsoftCanvas::width(const char *text, int font) const {
  return dots(text, font) / sx_;
}

const char *WsoftCanvas::fit(const char *text, int font, double max, char *out, size_t size) const {
  size_t n = 0;
  if (size == 0) return "";
  while (text[n] && n + 1 < size) {
    out[n] = text[n];
    n++;
  }
  out[n] = 0;
  while (n > 0 && width(out, font) > max) out[--n] = 0;
  return out;
}

double WsoftCanvas::text(const char *text, double x, double y, int font, uint16_t color, WsoftAlign align) {
  const WsoftFont &f = fonts_[font];
  int w = dots(text, font);
  long pen = rnd(x * sx_);
  if (align == WSOFT_RIGHT) pen -= w;
  else if (align == WSOFT_CENTER) pen -= (w + 1) >> 1;
  long baseline = rnd(y * sy_ + f.cap / 2.0);
  for (const char *p = text; *p; p++) {
    int k = (uint8_t)*p - f.first;
    if (k < 0 || k >= f.count) continue;
    const WsoftGlyph &g = f.glyphs[k];
    const uint8_t *rows = f.alpha + g.offset;
    int row_bytes = (g.w + 1) >> 1;
    for (int yy = 0; yy < g.h; yy++) {
      long py = baseline + g.dy + yy;
      if (py < top_ || py >= top_ + rows_) continue;
      const uint8_t *row = rows + yy * row_bytes;
      for (int xx = 0; xx < g.w; xx++) {
        long px = pen + g.dx + xx;
        int cover = (xx & 1) ? row[xx >> 1] & 15 : row[xx >> 1] >> 4;
        if (cover && px >= 0 && px < width_) put((int)px, (int)py, color, cover * 17);
      }
    }
    pen += g.adv;
  }
  return w / sx_;
}
