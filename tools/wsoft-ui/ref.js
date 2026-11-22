// W-Soft board screen: the reference drawing.
//
// This file is the screen as it must look, runnable in a browser. The firmware is a
// line-for-line port of it: lib/wsoft_core/src/wsoft_gfx.cpp (the drawing engine) and
// lib/wsoft_core/src/wsoft_ui_paint.cpp (what goes where). tools/wsoft-ui/gen.mjs runs this
// file in a browser and writes the letters, the colours, the test screens and the frame
// checksums the host test compares the firmware against (test/test_wsoft_ui).
//
// Rules that keep the two the same, dot for dot:
// - one layout in 320 x 240 units; a panel scales it (yellow board 1 x 1, Waveshare 2.5 x 2)
// - colours are 16-bit 5-6-5, blended per channel with the rounding in blend()
// - letters are 4-bit coverage maps, drawn without kerning
// - every row paints its own background first, so a row can be drawn again by itself
const WsoftUi = (() => {
  'use strict';

  const rnd = (v) => Math.floor(v + 0.5);

  // ---------- letters ----------
  // family, weight, size and letter spacing in layout units, first character and how many.
  const ROLES = [
    { name: 'TINY', family: 'Manrope', weight: 700, px: 11, track: 0.3, first: 32, count: 95 },
    { name: 'SMALL', family: 'Manrope', weight: 800, px: 13, track: 0.3, first: 32, count: 95 },
    { name: 'MID', family: 'Manrope', weight: 800, px: 18, track: 0, first: 32, count: 95 },
    { name: 'BIG', family: 'Manrope', weight: 800, px: 26, track: 0.4, first: 32, count: 59 },
    { name: 'HERO', family: 'Sora', weight: 800, px: 54, track: -1, first: 45, count: 13 },
    // The Full Colour screen: a bigger weight and a bigger Hi-Lo word.
    { name: 'HUGE', family: 'Sora', weight: 800, px: 76, track: -1.5, first: 45, count: 13 },
    { name: 'XL', family: 'Manrope', weight: 800, px: 34, track: 0.4, first: 32, count: 59 },
  ];
  const TINY = 0, SMALL = 1, MID = 2, BIG = 3, HERO = 4, HUGE = 5, XL = 6;
  const LEFT = 0, CENTER = 1, RIGHT = 2;

  // Rasterises one role at one scale with the browser's own font engine.
  function buildFont(roleIndex, scale) {
    const role = ROLES[roleIndex], px = Math.round(role.px * scale), size = px * 2 + 8;
    const c = document.createElement('canvas');
    c.width = size; c.height = size;
    const g = c.getContext('2d', { willReadFrequently: true });
    g.font = `${role.weight} ${px}px "${role.family}", "Segoe UI", sans-serif`;
    g.textBaseline = 'alphabetic';
    g.fillStyle = '#000';
    const base = Math.round(px * 1.45), ox = Math.round(px * 0.5);
    const font = { first: role.first, count: role.count, cap: 0, track: Math.round(role.track * scale), glyphs: [] };
    for (let i = 0; i < role.count; i++) {
      const ch = String.fromCharCode(role.first + i);
      g.clearRect(0, 0, size, size);
      g.fillText(ch, ox, base);
      const img = g.getImageData(0, 0, size, size).data;
      const a4 = (x, y) => Math.min(15, (img[(y * size + x) * 4 + 3] + 8) >> 4);
      let x0 = size, x1 = -1, y0 = size, y1 = -1;
      for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
        if (a4(x, y)) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
      }
      const adv = Math.max(0, rnd(g.measureText(ch).width + role.track * scale));
      if (x1 < 0) { font.glyphs.push({ w: 0, h: 0, dx: 0, dy: 0, adv, al: new Uint8Array(0) }); continue; }
      const w = x1 - x0 + 1, h = y1 - y0 + 1, al = new Uint8Array(w * h);
      for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) al[y * w + x] = a4(x0 + x, y0 + y);
      font.glyphs.push({ w, h, dx: x0 - ox, dy: y0 - base, adv, al });
    }
    // Text is centred on the height of a capital (a digit for the weight letters).
    const ref = font.glyphs[(role.first === 45 ? 48 : 72) - role.first];
    font.cap = -ref.dy;
    return font;
  }
  const buildFonts = (scale) => ROLES.map((_, i) => buildFont(i, scale));

  // Packed form for a file: per glyph w, h, dx, dy, adv and the coverage rows, 2 dots a byte.
  function packFont(font) {
    const glyphs = [], bytes = [];
    for (const gl of font.glyphs) {
      glyphs.push([bytes.length, gl.w, gl.h, gl.dx, gl.dy, gl.adv]);
      for (let y = 0; y < gl.h; y++) for (let x = 0; x < gl.w; x += 2) {
        bytes.push((gl.al[y * gl.w + x] << 4) | (x + 1 < gl.w ? gl.al[y * gl.w + x + 1] : 0));
      }
    }
    return { first: font.first, count: font.count, cap: font.cap, track: font.track, glyphs, bytes };
  }
  function unpackFont(p) {
    const font = { first: p.first, count: p.count, cap: p.cap, track: p.track, glyphs: [] };
    const bytes = p.bytes || Uint8Array.from(atob(p.b64), (ch) => ch.charCodeAt(0));
    for (const [off, w, h, dx, dy, adv] of p.glyphs) {
      const al = new Uint8Array(w * h), row = (w + 1) >> 1;
      for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
        const b = bytes[off + y * row + (x >> 1)];
        al[y * w + x] = x & 1 ? b & 15 : b >> 4;
      }
      font.glyphs.push({ w, h, dx, dy, adv, al });
    }
    return font;
  }

  // ---------- colours ----------
  const c565 = (hex) => ((parseInt(hex.slice(1, 3), 16) >> 3) << 11) | ((parseInt(hex.slice(3, 5), 16) >> 2) << 5) | (parseInt(hex.slice(5, 7), 16) >> 3);
  const mixHex = (a, b, t) => '#' + [1, 3, 5].map((i) => {
    const x = parseInt(a.slice(i, i + 2), 16), y = parseInt(b.slice(i, i + 2), 16);
    return Math.round(x + (y - x) * t).toString(16).padStart(2, '0');
  }).join('');
  // The Edge bench screen's colours (Weighsoft.Indicator.Edge packages/ui/src/theme.css and
  // operator.css). LOW is yellow, OK green, HIGH red: the same three as the Hi-Lo relays.
  const THEME_HEX = {
    dark: { bg: '#0b1620', panel: '#16273f', line: '#2b415c', ink: '#f5f8fa', soft: '#8fb8c8',
      ok: '#2fa46c', ok_ink: '#b7f0cd', wait: '#e2b84a', wait_ink: '#f3d48a', bad: '#dc5a5a', bad_ink: '#ffb4b4',
      chip_ok: mixHex('#0b1620', '#2fa46c', 0.18), chip_bad: mixHex('#0b1620', '#dc5a5a', 0.18),
      accent: '#2596be', btn: '#1e3654', btn_ink: '#f5f8fa', stop: '#c94545', pointer: '#f5f8fa',
      z_low: '#e2b84a', z_low_ink: '#1a2a33', z_ok: '#23965f', z_ok_ink: '#ffffff', z_high: '#c94545', z_high_ink: '#ffffff' },
    light: { bg: '#eef2f5', panel: '#ffffff', line: '#b4c0c8', ink: '#1a2a33', soft: '#4a6570',
      ok: '#15a34a', ok_ink: '#14532d', wait: '#e0a021', wait_ink: '#7a4d00', bad: '#dc2626', bad_ink: '#7f1d1d',
      chip_ok: mixHex('#eef2f5', '#15a34a', 0.18), chip_bad: mixHex('#eef2f5', '#dc2626', 0.18),
      accent: '#176685', btn: '#d3dce3', btn_ink: '#1a2a33', stop: '#dc2626', pointer: '#0f1e33',
      z_low: '#e9b227', z_low_ink: '#1a2a33', z_ok: '#15a34a', z_ok_ink: '#ffffff', z_high: '#dc2626', z_high_ink: '#ffffff' },
  };
  // The status bar is navy in both modes.
  const BAR_HEX = { bg: '#0f1e33', line: '#24354d', ink: '#f5f8fa', mute: '#8fb8c8', dim: '#3a506b', ok: '#3dbe78', bad: '#ff8a8a', warn: '#f2c879' };
  const to565 = (o) => Object.fromEntries(Object.entries(o).map(([k, v]) => [k, c565(v)]));
  const THEME = { dark: to565(THEME_HEX.dark), light: to565(THEME_HEX.light) };
  const BAR = to565(BAR_HEX);
  const WHITE = 0xffff, DARK = c565('#1a2a33');

  function blend(bg, fg, a) {
    if (a >= 255) return fg;
    const ia = 255 - a;
    const r = Math.floor((((fg >> 11) & 31) * a + ((bg >> 11) & 31) * ia + 127) / 255);
    const g = Math.floor((((fg >> 5) & 63) * a + ((bg >> 5) & 63) * ia + 127) / 255);
    const b = Math.floor(((fg & 31) * a + (bg & 31) * ia + 127) / 255);
    return (r << 11) | (g << 5) | b;
  }

  // ---------- the drawing engine ----------
  class Canvas {
    constructor(W, H, sx, sy, fonts) {
      this.W = W; this.H = H; this.sx = sx; this.sy = sy; this.fonts = fonts;
      this.fb = new Uint16Array(W * H);
      this.c0 = 0; this.c1 = H;
    }
    // Only the layout lines y0 up to y1 are drawn from here on: a row never paints outside itself.
    clip(y0, y1) {
      this.c0 = Math.max(rnd(y0 * this.sy), 0); this.c1 = Math.min(rnd(y1 * this.sy), this.H);
    }
    put(px, py, c, a) {
      const i = py * this.W + px;
      this.fb[i] = blend(this.fb[i], c, a);
    }
    // x, y, w, h in layout units. r: corner radius in units; below 0 = fully rounded ends.
    rect(x, y, w, h, c, r) {
      const X0 = rnd(x * this.sx), Y0 = rnd(y * this.sy), X1 = rnd((x + w) * this.sx), Y1 = rnd((y + h) * this.sy);
      let R = r < 0 ? Math.min(X1 - X0, Y1 - Y0) / 2 : r * this.sy;
      if (R > (X1 - X0) / 2) R = (X1 - X0) / 2;
      if (R > (Y1 - Y0) / 2) R = (Y1 - Y0) / 2;
      for (let py = Math.max(Y0, this.c0); py < Math.min(Y1, this.c1); py++) {
        let cornerRow = false, cy = 0;
        if (py + 0.5 < Y0 + R) { cornerRow = true; cy = Y0 + R; } else if (py + 0.5 > Y1 - R) { cornerRow = true; cy = Y1 - R; }
        for (let px = Math.max(X0, 0); px < Math.min(X1, this.W); px++) {
          let a = 255;
          if (cornerRow) {
            let corner = false, cx = 0;
            if (px + 0.5 < X0 + R) { corner = true; cx = X0 + R; } else if (px + 0.5 > X1 - R) { corner = true; cx = X1 - R; }
            if (corner) {
              const dx = px + 0.5 - cx, dy = py + 0.5 - cy, cov = R - Math.sqrt(dx * dx + dy * dy) + 0.5;
              if (cov <= 0) continue;
              if (cov < 1) a = rnd(cov * 255);
            }
          }
          this.put(px, py, c, a);
        }
      }
    }
    disc(cx, cy, r, c) {
      const X = cx * this.sx, Y = cy * this.sy, R = r * this.sy;
      for (let py = Math.max(Math.floor(Y - R - 1), this.c0); py <= Math.min(Math.floor(Y + R + 1), this.c1 - 1); py++) {
        for (let px = Math.max(Math.floor(X - R - 1), 0); px <= Math.min(Math.floor(X + R + 1), this.W - 1); px++) {
          const dx = px + 0.5 - X, dy = py + 0.5 - Y, cov = R - Math.sqrt(dx * dx + dy * dy) + 0.5;
          if (cov <= 0) continue;
          this.put(px, py, c, cov < 1 ? rnd(cov * 255) : 255);
        }
      }
    }
    // A line with round ends, w thick (the tick and the arrows of the Hi-Lo word).
    line(x1, y1, x2, y2, w, c) {
      const AX = x1 * this.sx, AY = y1 * this.sy, BX = x2 * this.sx, BY = y2 * this.sy, Hw = w * this.sy / 2;
      const ex = BX - AX, ey = BY - AY, len2 = ex * ex + ey * ey;
      const top = Math.floor(Math.min(AY, BY) - Hw - 1), bottom = Math.floor(Math.max(AY, BY) + Hw + 1);
      const left = Math.floor(Math.min(AX, BX) - Hw - 1), right = Math.floor(Math.max(AX, BX) + Hw + 1);
      for (let py = Math.max(top, this.c0); py <= Math.min(bottom, this.c1 - 1); py++) {
        for (let px = Math.max(left, 0); px <= Math.min(right, this.W - 1); px++) {
          const qx = px + 0.5 - AX, qy = py + 0.5 - AY;
          let t = len2 > 0 ? (qx * ex + qy * ey) / len2 : 0;
          if (t < 0) t = 0; else if (t > 1) t = 1;
          const dx = qx - t * ex, dy = qy - t * ey, cov = Hw - Math.sqrt(dx * dx + dy * dy) + 0.5;
          if (cov <= 0) continue;
          this.put(px, py, c, cov < 1 ? rnd(cov * 255) : 255);
        }
      }
    }
    // A piece of a ring: radius r, w thick inwards, from the direction (ax, ay) clockwise to
    // (bx, by), less than half a turn. Directions are unit lengths, y down. A circle on every
    // panel: the radius scales with the height, like disc().
    ring(cx, cy, r, w, ax, ay, bx, by, c) {
      const X = cx * this.sx, Y = cy * this.sy, R = r * this.sy, I = R - w * this.sy;
      const far = (R + 1) * (R + 1), near = I > 1 ? (I - 1) * (I - 1) : 0;
      for (let py = Math.max(Math.floor(Y - R - 1), this.c0); py <= Math.min(Math.floor(Y + R + 1), this.c1 - 1); py++) {
        for (let px = Math.max(Math.floor(X - R - 1), 0); px <= Math.min(Math.floor(X + R + 1), this.W - 1); px++) {
          const dx = px + 0.5 - X, dy = py + 0.5 - Y, d2 = dx * dx + dy * dy;
          if (d2 > far || d2 < near) continue;
          const d = Math.sqrt(d2);
          let cov = R - d + 0.5;
          const inner = d - I + 0.5, from = ax * dy - ay * dx + 0.5, to = dx * by - dy * bx + 0.5;
          if (inner < cov) cov = inner;
          if (from < cov) cov = from;
          if (to < cov) cov = to;
          if (cov <= 0) continue;
          this.put(px, py, c, cov < 1 ? rnd(cov * 255) : 255);
        }
      }
    }
    // A six-pointed star (two triangles), r = centre to a point. Edges by 4 x 4 samples a dot.
    star6(cx, cy, r, c) {
      const X = cx * this.sx, Y = cy * this.sy, R = r * this.sy;
      const inside = (x, y) => {
        const k = Math.sqrt(3) * Math.abs(x);
        return (y <= R / 2 && y + R >= k) || (y >= -R / 2 && R - y >= k);
      };
      for (let py = Math.max(Math.floor(Y - R - 1), this.c0); py <= Math.min(Math.floor(Y + R + 1), this.c1 - 1); py++) {
        for (let px = Math.max(Math.floor(X - R - 1), 0); px <= Math.min(Math.floor(X + R + 1), this.W - 1); px++) {
          let n = 0;
          for (let j = 0; j < 4; j++) {
            for (let i = 0; i < 4; i++) {
              if (inside(px + (i + 0.5) / 4 - X, py + (j + 0.5) / 4 - Y)) n++;
            }
          }
          if (n === 0) continue;
          this.put(px, py, c, n === 16 ? 255 : rnd(n * 255 / 16));
        }
      }
    }
    // Width in panel dots. Characters the font does not hold take no room.
    dots(str, role) {
      const f = this.fonts[role];
      let w = 0;
      for (let i = 0; i < str.length; i++) {
        const k = str.charCodeAt(i) - f.first;
        if (k >= 0 && k < f.count) w += f.glyphs[k].adv;
      }
      // The last letter's spacing is not part of the text.
      return w > 0 && w > f.track ? w - f.track : 0;
    }
    width(str, role) { return this.dots(str, role) / this.sx; }
    // The longest start of str that is no wider than max layout units.
    fit(str, role, max) {
      let n = str.length;
      while (n > 0 && this.width(str.slice(0, n), role) > max) n--;
      return str.slice(0, n);
    }
    // y is the middle of the capital letters. Returns the width in layout units.
    text(str, x, y, role, c, align) {
      const f = this.fonts[role], w = this.dots(str, role);
      let pen = rnd(x * this.sx);
      if (align === RIGHT) pen -= w; else if (align === CENTER) pen -= (w + 1) >> 1;
      const baseline = rnd(y * this.sy + f.cap / 2);
      for (let i = 0; i < str.length; i++) {
        const k = str.charCodeAt(i) - f.first;
        if (k < 0 || k >= f.count) continue;
        const gl = f.glyphs[k];
        for (let yy = 0; yy < gl.h; yy++) {
          const py = baseline + gl.dy + yy;
          if (py < this.c0 || py >= this.c1) continue;
          for (let xx = 0; xx < gl.w; xx++) {
            const px = pen + gl.dx + xx, al = gl.al[yy * gl.w + xx];
            if (al && px >= 0 && px < this.W) this.put(px, py, c, al * 17);
          }
        }
        pen += gl.adv;
      }
      return w / this.sx;
    }
  }

  // ---------- the screen model: every word and state the screen shows ----------
  const PAGE_LIVE = 0, PAGE_MENU = 1, PAGE_PIN = 2, PAGE_NO_PIN = 3, PAGE_WIFI = 4;
  const TONE_SOFT = 0, TONE_OK = 1, TONE_WAIT = 2, TONE_BAD = 3;
  const SLOT_NONE = 0, SLOT_STOP = 1, SLOT_WAIT = 2, SLOT_WIFI = 3;
  const STAR_W = 20;  // the not-for-trade star (WSOFT_UI_STAR_W)
  const ZONE_NONE = 0, ZONE_LOW = 1, ZONE_OK = 2, ZONE_HIGH = 3;
  const LOOK_BTN = 0, LOOK_ON = 1, LOOK_ACCENT = 2, LOOK_PLAIN = 3, LOOK_BAD = 4;
  // The style of the live page: the classic rows, the whole screen in the Hi-Lo colour, or a dial.
  const STYLE_CLASSIC = 0, STYLE_COLOUR = 1, STYLE_DIAL = 2;

  // ---------- where things are (layout units) ----------
  // Tappable boxes: x, y, w, h. The firmware uses the same numbers for its taps.
  const BOX = {
    wifiBar: [0, 0, 150, 20], slot: [6, 22, 308, 40], connect: [160, 26, 100, 32], dismiss: [266, 26, 42, 32],
    gross: [8, 190, 104, 46], zero: [8, 190, 104, 46], menu: [120, 190, 72, 46], save: [200, 190, 112, 46],
    back: [164, 190, 148, 44], wide: [8, 190, 304, 44],
  };
  const tileBox = (i) => [i % 2 ? 164 : 8, 46 + (i >> 1) * 48, 148, 44];
  const KEY_X = [8, 86, 164, 242];
  const keyBox = (col, row, cols, rows) => [KEY_X[col], 46 + row * 48, 72 * cols + 6 * (cols - 1), 44 * rows + 4 * (rows - 1)];

  const tone = (T, t) => (t === TONE_OK ? T.ok_ink : t === TONE_WAIT ? T.wait_ink : t === TONE_BAD ? T.bad_ink : T.soft);
  const dot = (T, t) => (t === TONE_OK ? T.ok : t === TONE_WAIT ? T.wait : t === TONE_BAD ? T.bad : T.line);
  const barTone = (t) => (t === TONE_OK ? BAR.ok : t === TONE_WAIT ? BAR.warn : t === TONE_BAD ? BAR.bad : BAR.mute);

  // ---------- the live page, row by row ----------
  function paintBar(c, m, bg, line) {
    c.rect(0, 0, 320, 20, bg, 0);
    c.rect(0, 19, 320, 1, line, 0);
    for (let i = 0; i < 4; i++) {
      const h = 4 + i * 3;
      c.rect(8 + i * 5, 15 - h, 3, h, m.wifi_up && i < m.wifi_bars ? BAR.ok : BAR.dim, 0);
    }
    // The address is never cut: a cut address is another address. When the bar is too full,
    // the Hub's words give way and only its dot tells its state.
    const full = 32 + c.width(m.ip, SMALL) <= 312 - c.width(m.link, SMALL) - 20 - c.width(m.hub, SMALL) - 16.5;
    let x = 312;
    x -= c.text(m.link, x, 10, SMALL, m.link_tone === TONE_OK ? BAR.ink : barTone(m.link_tone), RIGHT);
    c.disc(x - 7, 10, 3.5, barTone(m.link_tone));
    x -= 20;
    x -= c.text(full ? m.hub : 'HUB', x, 10, SMALL, m.hub_tone === TONE_OK ? BAR.ink : barTone(m.hub_tone), RIGHT);
    c.disc(x - 7, 10, 3.5, barTone(m.hub_tone));
    c.text(m.ip, 32, 10, SMALL, m.wifi_up ? BAR.ink : BAR.bad, LEFT);
  }

  // The plate beside the audit counter. Its limits are never cut: a cut number would be a
  // wrong number. A long SA number is cut and marked with dots. If even the limits do not
  // fit, whole words only, marked too.
  function plateLine(c, sa, limits, room) {
    if (!sa.length && !limits.length) return '';
    const tail = '   ' + limits;
    if (c.width(sa + tail, TINY) <= room) return sa + tail;
    const short = c.fit(sa, TINY, room - c.width('...' + tail, TINY));
    if (short.length > 3) return short + '...' + tail;
    if (c.width(limits, TINY) <= room) return limits;
    let cut = limits;
    while (cut.length && c.width(cut + ' ...', TINY) > room) cut = cut.slice(0, Math.max(0, cut.lastIndexOf(' ')));
    return cut + ' ...';
  }

  // The band over the trade row: the Wi-Fi alert or a save notice. False when there is none.
  function paintSlot(c, m, T) {
    if (m.slot === SLOT_WIFI) {
      c.rect(6, 22, 308, 40, T.wait, 8);
      c.text('WI-FI IS OFF', 18, 42, MID, DARK, LEFT);
      c.rect(160, 26, 100, 32, DARK, 6);
      c.text('CONNECT', 210, 42, SMALL, WHITE, CENTER);
      c.rect(266, 26, 42, 32, DARK, 6);
      c.rect(267.5, 27.5, 39, 29, T.wait, 5);
      c.text('X', 287, 42, MID, DARK, CENTER);
      return true;
    }
    if (m.slot !== SLOT_NONE) {
      const stop = m.slot === SLOT_STOP, ink = stop ? WHITE : DARK;
      c.rect(6, 22, 308, 40, stop ? T.stop : T.wait, 8);
      if (c.width(m.slot_title, BIG) <= 292) c.text(m.slot_title, 160, 34, BIG, ink, CENTER);
      else c.text(c.fit(m.slot_title, MID, 296), 160, 34, MID, ink, CENTER);
      c.text(c.fit(m.slot_detail, SMALL, 296), 160, 53, SMALL, ink, CENTER);
      return true;
    }
    return false;
  }

  function paintTop(c, m, T) {
    c.rect(0, 20, 320, 42, T.bg, 0);
    if (paintSlot(c, m, T)) return;
    // Not for trade: a small six-pointed star instead of the words (Rhyno, 14 Nov 2026).
    let w = STAR_W;
    if (m.trade_ok) {
      const chip = 'TRADE CHECKS ON';
      w = c.width(chip, SMALL) + 14;
      c.rect(8, 24, w, 17, T.ok, 4);
      c.rect(9, 25, w - 2, 15, T.chip_ok, 3);
      c.text(chip, 8 + w / 2, 32.5, SMALL, T.ok_ink, CENTER);
    } else {
      c.star6(8 + w / 2, 32.5, 9, T.bad);
    }
    const sw = c.text(m.steady, 312, 32.5, SMALL, tone(T, m.steady_tone), RIGHT);
    c.disc(312 - sw - 8, 32.5, 4, dot(T, m.steady_tone));
    c.text(c.fit(m.reason, TINY, 312 - sw - 20 - (8 + w + 7)), 8 + w + 7, 32.5, TINY, tone(T, m.reason_tone), LEFT);
    const aw = c.text(m.audit, 312, 52, TINY, T.soft, RIGHT);
    c.text(plateLine(c, m.plate_sa, m.plate_limits, 312 - aw - 8 - 10), 10, 52, TINY, T.soft, LEFT);
  }

  // The weight panel takes the colour of the Hi-Lo zone the relays are in: yellow LOW, green
  // OK, red HIGH. No zone (no product, empty scale, no signal): the plain background.
  function paintWeight(c, m, T) {
    c.rect(0, 62, 320, 60, T.bg, 0);
    let ink = m.live ? T.ink : T.soft, soft = T.soft;
    if (m.zone !== ZONE_NONE) {
      c.rect(6, 64, 308, 56, m.zone === ZONE_LOW ? T.z_low : m.zone === ZONE_OK ? T.z_ok : T.z_high, 8);
      ink = soft = m.zone === ZONE_LOW ? T.z_low_ink : m.zone === ZONE_OK ? T.z_ok_ink : T.z_high_ink;
    }
    c.text(m.label, 14, 72, TINY, soft, LEFT);
    c.text(m.weight, 268, 95, c.width(m.weight, HERO) <= 250 ? HERO : BIG, ink, RIGHT);
    c.text(m.unit, 274, 106, MID, soft, LEFT);
  }

  function paintHilo(c, m, T) {
    c.rect(0, 122, 320, 66, T.bg, 0);
    c.disc(16, 137, 5, dot(T, m.word_tone));
    const ww = c.text(m.word, 28, 137, m.product_on ? BIG : MID, tone(T, m.word_tone), LEFT);
    // What is left of the line for the product, right of the word.
    const room = 310 - (28 + ww + 10);
    const bx = 10, bw = 300, by = 157, bh = 12;
    if (!m.product_on) {
      c.text(c.fit(m.target, TINY, room), 310, 137, TINY, T.soft, RIGHT);
      c.rect(bx, by, bw, bh, T.line, -1);
      return;
    }
    c.text(c.fit(m.product, SMALL, room), 310, 129.5, SMALL, T.ink, RIGHT);
    c.text(c.fit(m.target, TINY, room), 310, 143, TINY, T.soft, RIGHT);
    // The bar: LOW, OK and HIGH. The limits sit 6/22 and 16/22 along it, as on the Edge.
    const lo = bx + bw * 6 / 22, hi = bx + bw * 16 / 22;
    c.rect(bx, by, bw, bh, T.bad, -1);
    c.rect(bx, by, lo - bx + bh, bh, T.wait, -1);
    c.rect(lo, by, hi - lo, bh, T.ok, 0);
    c.text(m.low, lo, 181, TINY, T.soft, CENTER);
    c.text(m.high, hi, 181, TINY, T.soft, CENTER);
    if (m.pointer < 0) return;
    let mx = bx + bw * m.pointer / 1000;
    if (mx < bx + 4) mx = bx + 4;
    if (mx > bx + bw - 4) mx = bx + bw - 4;
    c.rect(mx - 6, by - 7, 12, bh + 14, T.bg, -1);
    c.rect(mx - 4, by - 5, 8, bh + 10, T.pointer, -1);
  }

  function paintBottom(c, m, T) {
    c.rect(0, 188, 320, 52, T.bg, 0);
    if (m.zero_btn) {
      // A board that weighs itself (load cell): ZERO takes the place of the GROSS read-out
      // (with no tare on, gross is the big number anyway).
      c.rect(8, 190, 104, 46, T.btn, 8);
      c.text('ZERO', 60, 206, MID, T.btn_ink, CENTER);
      if (c.width(m.zero_line, SMALL) <= 96) c.text(m.zero_line, 60, 225, SMALL, T.btn_ink, CENTER);
      else c.text(c.fit(m.zero_line, TINY, 96), 60, 225, TINY, T.btn_ink, CENTER);
    } else {
      c.rect(8, 190, 104, 46, T.line, 8);
      c.rect(9, 191, 102, 44, T.panel, 7);
      c.text('GROSS', 17, 202, TINY, T.soft, LEFT);
      if (c.width(m.gross, MID) <= 88) c.text(m.gross, 17, 221, MID, m.live ? T.ink : T.soft, LEFT);
      else c.text(c.fit(m.gross, SMALL, 88), 17, 221, SMALL, m.live ? T.ink : T.soft, LEFT);
    }
    c.rect(120, 190, 72, 46, T.btn, 8);
    for (let i = 0; i < 3; i++) c.rect(144, 200 + i * 6, 24, 3, T.btn_ink, 1);
    c.text('MENU', 156, 226, SMALL, T.btn_ink, CENTER);
    c.rect(200, 190, 112, 46, m.save_look === LOOK_ON ? T.ok : m.save_look === LOOK_BAD ? T.stop : T.btn, 8);
    const ink = m.save_look === LOOK_BTN ? T.btn_ink : WHITE;
    c.text('SAVE', 256, 206, MID, ink, CENTER);
    if (c.width(m.save_line, SMALL) <= 104) c.text(m.save_line, 256, 225, SMALL, ink, CENTER);
    else c.text(c.fit(m.save_line, TINY, 104), 256, 225, TINY, ink, CENTER);
  }

  // ---------- the Hi-Lo word's sign: a tick, or an arrow down (low) or up (high) ----------
  function paintSign(c, zone, x, y, ink) {
    if (zone === ZONE_OK) {
      c.line(x - 8, y, x - 2.5, y + 5.5, 4, ink);
      c.line(x - 2.5, y + 5.5, x + 8, y - 6, 4, ink);
    } else if (zone === ZONE_LOW) {
      c.line(x - 7.5, y - 4, x, y + 4.5, 4, ink);
      c.line(x, y + 4.5, x + 7.5, y - 4, 4, ink);
    } else {
      c.line(x - 7.5, y + 4, x, y - 4.5, 4, ink);
      c.line(x, y - 4.5, x + 7.5, y + 4, 4, ink);
    }
  }
  // The plate and the audit counter belong on the screen when there is a plate or the trade checks are on.
  const plateOn = (m) => m.trade_ok || m.plate_sa.length > 0 || m.plate_limits.length > 0;

  // ---------- Full Colour: the whole screen takes the colour of the Hi-Lo zone ----------
  // With no zone (no product, empty scale, no signal) it is the classic dark screen with the
  // bigger weight.
  function colourSet(m, T) {
    const bg = m.zone === ZONE_LOW ? T.z_low : m.zone === ZONE_OK ? T.z_ok : T.z_high;
    const ink = m.zone === ZONE_LOW ? T.z_low_ink : m.zone === ZONE_OK ? T.z_ok_ink : T.z_high_ink;
    const pale = ink !== WHITE;
    return { bg, ink, soft: blend(bg, ink, 184), shade: blend(bg, 0, 82), bar: blend(bg, 0, 115), on: pale ? DARK : WHITE, on_ink: pale ? WHITE : DARK };
  }

  function paintTopColour(c, m, T) {
    if (m.zone === ZONE_NONE) return paintTop(c, m, T);
    const P = colourSet(m, T);
    c.rect(0, 20, 320, 42, P.bg, 0);
    if (m.slot === SLOT_WIFI) {
      c.rect(6, 22, 308, 40, P.shade, 8);
      c.text('WI-FI IS OFF', 18, 42, MID, WHITE, LEFT);
      c.rect(160, 26, 100, 32, WHITE, 6);
      c.text('CONNECT', 210, 42, SMALL, DARK, CENTER);
      c.rect(266, 26, 42, 32, WHITE, 6);
      c.rect(267.5, 27.5, 39, 29, P.shade, 5);
      c.text('X', 287, 42, MID, WHITE, CENTER);
      return;
    }
    if (m.slot !== SLOT_NONE) {
      c.rect(6, 22, 308, 40, m.slot === SLOT_STOP ? DARK : P.shade, 8);
      if (c.width(m.slot_title, BIG) <= 292) c.text(m.slot_title, 160, 34, BIG, WHITE, CENTER);
      else c.text(c.fit(m.slot_title, MID, 296), 160, 34, MID, WHITE, CENTER);
      c.text(c.fit(m.slot_detail, SMALL, 296), 160, 53, SMALL, WHITE, CENTER);
      return;
    }
    let w = STAR_W;
    if (m.trade_ok) {
      const chip = 'TRADE CHECKS ON';
      w = c.width(chip, SMALL) + 14;
      c.rect(8, 24, w, 17, P.shade, 4);
      c.text(chip, 8 + w / 2, 32.5, SMALL, WHITE, CENTER);
    } else {
      c.star6(8 + w / 2, 32.5, 9, P.ink);
    }
    const sw = c.text(m.steady, 312, 32.5, SMALL, P.ink, RIGHT);
    c.disc(312 - sw - 8, 32.5, 4, P.ink);
    c.text(c.fit(m.reason, TINY, 312 - sw - 20 - (8 + w + 7)), 8 + w + 7, 32.5, TINY, P.soft, LEFT);
    if (plateOn(m)) {
      const aw = c.text(m.audit, 312, 52, TINY, P.soft, RIGHT);
      c.text(plateLine(c, m.plate_sa, m.plate_limits, 312 - aw - 8 - 10), 10, 52, TINY, P.soft, LEFT);
      return;
    }
    const tw = c.text(m.target, 312, 52, SMALL, P.soft, RIGHT);
    c.text(c.fit(m.product, MID, 312 - tw - 12 - 10), 10, 51, MID, P.ink, LEFT);
  }

  function paintWeightColour(c, m, T) {
    let bg = T.bg, ink = m.live ? T.ink : T.soft, soft = T.soft;
    if (m.zone !== ZONE_NONE) {
      const P = colourSet(m, T);
      bg = P.bg; ink = P.ink; soft = P.soft;
    }
    c.rect(0, 62, 320, 60, bg, 0);
    if (c.width(m.label, TINY) <= 24 && c.width(m.weight, HUGE) <= 276) {
      c.text(m.label, 312, 71, TINY, soft, RIGHT);
      c.text(m.weight, 284, 92, HUGE, ink, RIGHT);
      c.text(m.unit, 290, 109, MID, soft, LEFT);
      return;
    }
    c.text(m.label, 14, 72, TINY, soft, LEFT);
    c.text(m.weight, 268, 95, c.width(m.weight, HERO) <= 250 ? HERO : BIG, ink, RIGHT);
    c.text(m.unit, 274, 106, MID, soft, LEFT);
  }

  function paintHiloColour(c, m, T) {
    if (m.zone === ZONE_NONE) return paintHilo(c, m, T);
    const P = colourSet(m, T);
    c.rect(0, 122, 320, 66, P.bg, 0);
    const role = c.width(m.word, XL) <= 250 ? XL : BIG;
    const x0 = 160 - (c.width(m.word, role) + 26) / 2;
    paintSign(c, m.zone, x0 + 9, 139, P.ink);
    c.text(m.word, x0 + 26, 139, role, P.ink, LEFT);
    // Under the word: what to do. The product moves here when the plate has its line.
    if (plateOn(m)) c.text(c.fit(m.product + '   ' + m.target, SMALL, 300), 160, 160, SMALL, P.soft, CENTER);
    else c.text(m.zone === ZONE_LOW ? 'ADD MORE' : m.zone === ZONE_OK ? 'INSIDE THE LIMITS' : 'TAKE SOME OUT', 160, 160, SMALL, P.soft, CENTER);
    const bx = 12, bw = 296, by = 170, bh = 8;
    const lo = bx + bw * 6 / 22, hi = bx + bw * 16 / 22;
    c.rect(bx, by, bw, bh, P.shade, -1);
    c.rect(lo, by, hi - lo, bh, P.soft, 0);
    c.text(m.low, lo, 183.5, TINY, P.soft, CENTER);
    c.text(m.high, hi, 183.5, TINY, P.soft, CENTER);
    if (m.pointer < 0) return;
    let mx = bx + bw * m.pointer / 1000;
    if (mx < bx + 7) mx = bx + 7;
    if (mx > bx + bw - 7) mx = bx + bw - 7;
    c.disc(mx, by + bh / 2, 7, P.on);
    c.disc(mx, by + bh / 2, 3.5, P.bg);
  }

  function paintBottomColour(c, m, T) {
    if (m.zone === ZONE_NONE) return paintBottom(c, m, T);
    const P = colourSet(m, T);
    c.rect(0, 188, 320, 52, P.bg, 0);
    c.rect(8, 190, 104, 46, P.shade, 8);
    if (m.zero_btn) {
      c.text('ZERO', 60, 206, MID, WHITE, CENTER);
      if (c.width(m.zero_line, SMALL) <= 96) c.text(m.zero_line, 60, 225, SMALL, WHITE, CENTER);
      else c.text(c.fit(m.zero_line, TINY, 96), 60, 225, TINY, WHITE, CENTER);
    } else {
      c.text('GROSS', 17, 202, TINY, WHITE, LEFT);
      if (c.width(m.gross, MID) <= 88) c.text(m.gross, 17, 221, MID, WHITE, LEFT);
      else c.text(c.fit(m.gross, SMALL, 88), 17, 221, SMALL, WHITE, LEFT);
    }
    c.rect(120, 190, 72, 46, P.shade, 8);
    for (let i = 0; i < 3; i++) c.rect(144, 200 + i * 6, 24, 3, WHITE, 1);
    c.text('MENU', 156, 226, SMALL, WHITE, CENTER);
    c.rect(200, 190, 112, 46, m.save_look === LOOK_ON ? P.on : m.save_look === LOOK_BAD ? DARK : P.shade, 8);
    const ink = m.save_look === LOOK_ON ? P.on_ink : WHITE;
    c.text('SAVE', 256, 206, MID, ink, CENTER);
    if (c.width(m.save_line, SMALL) <= 104) c.text(m.save_line, 256, 225, SMALL, ink, CENTER);
    else c.text(c.fit(m.save_line, TINY, 104), 256, 225, TINY, ink, CENTER);
  }

  // ---------- Dial: the Hi-Lo bar bent into a gauge over the weight ----------
  const DIAL_X = 160, DIAL_Y = 176, DIAL_R = 152, DIAL_W = 11;
  // The direction from the dial's centre for a place 0..1000 along it: from the left end over
  // the top to the right end. Square roots only, so a PC and a board find the same dot.
  function dialDir(p) {
    const ex = 0.9703, ey = -0.2419;
    let vx, vy;
    if (p < 500) { const u = p / 500; vx = -ex + ex * u; vy = ey + (-1 - ey) * u; }
    else { const u = (p - 500) / 500; vx = ex * u; vy = -1 + (ey + 1) * u; }
    const n = Math.sqrt(vx * vx + vy * vy);
    return [vx / n, vy / n];
  }
  // The arc, the two limits and the marker. Three rows share it: each paints its own part.
  function paintDial(c, m, T) {
    const a = dialDir(0), lo = dialDir(6000 / 22), hi = dialDir(16000 / 22), b = dialDir(1000);
    if (!m.product_on) {
      c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, a[0], a[1], 0, -1, T.line);
      c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, 0, -1, b[0], b[1], T.line);
      return;
    }
    c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, a[0], a[1], lo[0], lo[1], T.wait);
    c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, lo[0], lo[1], hi[0], hi[1], T.ok);
    c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, hi[0], hi[1], b[0], b[1], T.bad);
    const k = c.sy / c.sx, lr = DIAL_R - DIAL_W - 12;
    c.text(m.low, DIAL_X + lo[0] * lr * k, DIAL_Y + lo[1] * lr, TINY, T.soft, CENTER);
    c.text(m.high, DIAL_X + hi[0] * lr * k, DIAL_Y + hi[1] * lr, TINY, T.soft, CENTER);
    if (m.pointer < 0) return;
    const d = dialDir(m.pointer), mr = DIAL_R - DIAL_W / 2;
    const mx = DIAL_X + d[0] * mr * k, my = DIAL_Y + d[1] * mr;
    c.disc(mx, my, 10, T.bg);
    c.disc(mx, my, 8, T.pointer);
    c.disc(mx, my, 3.5, m.zone === ZONE_LOW ? T.z_low : m.zone === ZONE_OK ? T.z_ok : m.zone === ZONE_HIGH ? T.z_high : T.bg);
  }

  function paintTopDial(c, m, T) {
    c.rect(0, 20, 320, 42, T.bg, 0);
    paintDial(c, m, T);
    if (paintSlot(c, m, T)) return;
    if (m.trade_ok) {
      // Two short lines: the dial leaves no room for one long one.
      c.rect(8, 24, 68, 24, T.ok, 4);
      c.rect(9, 25, 66, 22, T.chip_ok, 3);
      c.text('TRADE', 42, 30.5, TINY, T.ok_ink, CENTER);
      c.text('CHECKS ON', 42, 41.5, TINY, T.ok_ink, CENTER);
      c.text(c.fit(m.reason, TINY, 48), 10, 56, TINY, tone(T, m.reason_tone), LEFT);
    } else {
      c.star6(18, 32.5, 9, T.bad);
      c.text(c.fit(m.reason, TINY, 66), 35, 32.5, TINY, tone(T, m.reason_tone), LEFT);
    }
    const sw = c.text(m.steady, 312, 32.5, SMALL, tone(T, m.steady_tone), RIGHT);
    c.disc(312 - sw - 8, 32.5, 4, dot(T, m.steady_tone));
  }

  function paintWeightDial(c, m, T) {
    c.rect(0, 62, 320, 60, T.bg, 0);
    paintDial(c, m, T);
    c.text(m.label + '  ' + m.unit, 160, 71, TINY, T.soft, CENTER);
    c.text(m.weight, 160, 100, c.width(m.weight, HERO) <= 200 ? HERO : BIG, m.live ? T.ink : T.soft, CENTER);
  }

  function paintHiloDial(c, m, T) {
    c.rect(0, 122, 320, 66, T.bg, 0);
    paintDial(c, m, T);
    // The word in a pill of the zone's colour, with its sign.
    const zoned = m.zone !== ZONE_NONE;
    const fill = m.zone === ZONE_LOW ? T.z_low : m.zone === ZONE_OK ? T.z_ok : m.zone === ZONE_HIGH ? T.z_high : T.btn;
    const ink = m.zone === ZONE_LOW ? T.z_low_ink : m.zone === ZONE_OK ? T.z_ok_ink : m.zone === ZONE_HIGH ? T.z_high_ink : tone(T, m.word_tone);
    const role = m.product_on ? BIG : MID, sign = zoned ? 24 : 0;
    const pw = c.width(m.word, role) + sign + 28, px = 160 - pw / 2;
    c.rect(px, 141, pw, 25, fill, -1);
    if (zoned) paintSign(c, m.zone, px + 23, 153.5, ink);
    c.text(m.word, px + 14 + sign, 153.5, role, ink, LEFT);
    // The plate takes the bottom line when it is there: the product line moves up in small letters.
    const role2 = plateOn(m) ? TINY : SMALL, y2 = plateOn(m) ? 171.5 : 174;
    if (m.product_on) c.text(c.fit(m.product + '   ' + m.target, role2, 300), 160, y2, role2, T.soft, CENTER);
    else c.text(c.fit(m.target, role2, 300), 160, y2, role2, T.soft, CENTER);
    if (!plateOn(m)) return;
    const aw = c.text(m.audit, 312, 183.5, TINY, T.soft, RIGHT);
    c.text(plateLine(c, m.plate_sa, m.plate_limits, 312 - aw - 8 - 10), 10, 183.5, TINY, T.soft, LEFT);
  }

  // ---------- the menu pages (everything under the status bar) ----------
  function paintTile(c, T, box, caption, value, look) {
    const [x, y, w, h] = box;
    const strong = look === LOOK_ON || look === LOOK_ACCENT;
    if (look === LOOK_PLAIN) {
      c.rect(x, y, w, h, T.line, 8);
      c.rect(x + 1, y + 1, w - 2, h - 2, T.panel, 7);
    } else {
      c.rect(x, y, w, h, look === LOOK_ON ? T.ok : look === LOOK_ACCENT ? T.accent : T.btn, 8);
    }
    const ink = strong ? WHITE : look === LOOK_PLAIN ? T.ink : T.btn_ink;
    if (caption.length) {
      c.text(caption, x + 10, y + 13, TINY, strong ? WHITE : T.soft, LEFT);
      if (c.width(value, MID) <= w - 20) c.text(value, x + 10, y + 30, MID, ink, LEFT);
      else c.text(c.fit(value, SMALL, w - 20), x + 10, y + 30, SMALL, ink, LEFT);
    } else {
      c.text(value, x + w / 2, y + h / 2, MID, ink, CENTER);
    }
  }

  function paintPage(c, m, T) {
    c.rect(0, 20, 320, 220, T.bg, 0);
    const tw = c.text(m.title, 10, 32, SMALL, T.ink, LEFT);
    c.text(c.fit(m.message, SMALL, 310 - (10 + tw + 12 + m.pin_count * 14)), 310, 32, SMALL, T.wait_ink, RIGHT);
    if (m.page === PAGE_MENU) {
      for (let i = 0; i < m.tile_count; i++) paintTile(c, T, tileBox(i), m.tiles[i].caption, m.tiles[i].value, m.tiles[i].look);
      paintTile(c, T, BOX.back, '', 'BACK', LOOK_PLAIN);
    } else if (m.page === PAGE_PIN) {
      for (let i = 0; i < m.pin_count; i++) c.disc(100 + i * 14, 32, 4, T.ink);
      for (let i = 0; i < 9; i++) paintTile(c, T, keyBox(i % 3, Math.floor(i / 3), 1, 1), '', String.fromCharCode(49 + i), LOOK_BTN);
      paintTile(c, T, keyBox(0, 3, 3, 1), '', '0', LOOK_BTN);
      paintTile(c, T, keyBox(3, 0, 1, 1), '', 'DEL', LOOK_BTN);
      paintTile(c, T, keyBox(3, 1, 1, 1), '', 'BACK', LOOK_PLAIN);
      paintTile(c, T, keyBox(3, 2, 1, 2), '', 'OK', LOOK_ON);
    } else if (m.page === PAGE_NO_PIN) {
      c.text('SET A SCREEN PIN FIRST', 160, 62, MID, T.bad_ink, CENTER);
      if (m.address.length) {
        c.text('On a phone or PC, open', 160, 90, SMALL, T.soft, CENTER);
        c.text(m.address, 160, 112, MID, T.ink, CENTER);
      } else {
        // No Wi-Fi and no hotspot: there is no address to give yet.
        c.text('Connect the board to Wi-Fi,', 160, 90, SMALL, T.ink, CENTER);
        c.text('then open its web page', 160, 112, SMALL, T.ink, CENTER);
      }
      c.text('Log in as admin, then', 160, 140, SMALL, T.soft, CENTER);
      c.text('W-Soft  >  Settings  >  Approval PIN', 160, 160, SMALL, T.ink, CENTER);
      paintTile(c, T, BOX.wide, '', 'BACK', LOOK_PLAIN);
    } else if (m.wifi_up) {
      c.text('CONNECTED', 160, 84, BIG, T.ok_ink, CENTER);
      c.text(m.ip, 160, 118, MID, T.ink, CENTER);
      c.text('Type this address in a browser on the same Wi-Fi', 160, 144, TINY, T.soft, CENTER);
      paintTile(c, T, BOX.wide, '', 'BACK', LOOK_PLAIN);
    } else if (!m.ap_name.length) {
      // No Wi-Fi and the board's own hotspot is off: nothing to join.
      c.text('NO WI-FI', 160, 84, BIG, T.bad_ink, CENTER);
      c.text('The board hotspot is off too.', 160, 118, SMALL, T.ink, CENTER);
      c.text('Restart the board to open its hotspot', 160, 144, TINY, T.soft, CENTER);
      paintTile(c, T, BOX.wide, '', 'BACK', LOOK_PLAIN);
    } else {
      for (let i = 0; i < 3; i++) {
        c.disc(22, 58 + i * 40, 10, T.accent);
        c.text(String.fromCharCode(49 + i), 22, 58 + i * 40, SMALL, WHITE, CENTER);
      }
      c.text('On your phone, join this Wi-Fi', 42, 50, TINY, T.soft, LEFT);
      if (c.width(m.ap_name, MID) <= 268) c.text(m.ap_name, 42, 67, MID, T.ink, LEFT);
      else c.text(c.fit(m.ap_name, SMALL, 268), 42, 67, SMALL, T.ink, LEFT);
      c.text('Then open this address', 42, 90, TINY, T.soft, LEFT);
      c.text(m.address, 42, 107, MID, T.ink, LEFT);
      c.text('Log in, tap Wi-Fi and pick yours', 42, 138, SMALL, T.ink, LEFT);
      c.text('The board keeps looking for its saved Wi-Fi by itself', 42, 170, TINY, T.wait_ink, LEFT);
      paintTile(c, T, BOX.wide, '', 'BACK', LOOK_PLAIN);
    }
  }

  // The rows of the screen: first and last layout line, and what draws them.
  const ROW_BAR = 0, ROW_TOP = 1, ROW_WEIGHT = 2, ROW_HILO = 3, ROW_BOTTOM = 4, ROW_PAGE = 5;
  const ROWS = [[0, 20], [20, 62], [62, 122], [122, 188], [188, 240], [20, 240]];
  function paintRow(c, m, row) {
    // Full Colour and Dial are dark screens. A menu page looks the same in every style.
    const style = m.page === PAGE_LIVE ? m.style : STYLE_CLASSIC;
    const T = m.dark || m.style !== STYLE_CLASSIC ? THEME.dark : THEME.light;
    c.clip(ROWS[row][0], ROWS[row][1]);
    if (row === ROW_BAR) {
      if (style === STYLE_COLOUR && m.zone !== ZONE_NONE) { const bg = colourSet(m, T).bar; paintBar(c, m, bg, bg); }
      else paintBar(c, m, BAR.bg, BAR.line);
    } else if (row === ROW_TOP) {
      if (style === STYLE_COLOUR) paintTopColour(c, m, T); else if (style === STYLE_DIAL) paintTopDial(c, m, T); else paintTop(c, m, T);
    } else if (row === ROW_WEIGHT) {
      if (style === STYLE_COLOUR) paintWeightColour(c, m, T); else if (style === STYLE_DIAL) paintWeightDial(c, m, T); else paintWeight(c, m, T);
    } else if (row === ROW_HILO) {
      if (style === STYLE_COLOUR) paintHiloColour(c, m, T); else if (style === STYLE_DIAL) paintHiloDial(c, m, T); else paintHilo(c, m, T);
    } else if (row === ROW_BOTTOM) {
      if (style === STYLE_COLOUR) paintBottomColour(c, m, T); else paintBottom(c, m, T);
    } else paintPage(c, m, T);
    c.clip(0, 240);
  }
  function paint(c, m) {
    paintRow(c, m, ROW_BAR);
    if (m.page !== PAGE_LIVE) return paintRow(c, m, ROW_PAGE);
    for (let row = ROW_TOP; row <= ROW_BOTTOM; row++) paintRow(c, m, row);
  }

  // ---------- from a plain state to the model (the firmware builds its model from its services) ----------
  const NOTICES = {
    NEED_ZERO: [SLOT_STOP, 'TAKE THE LOAD OFF', 'The scale must show 0 kg first'],
    NOT_STABLE: [SLOT_WAIT, 'WEIGHT IS MOVING', 'Keep the load still'],
    NO_SIGNAL: [SLOT_STOP, 'NO SCALE SIGNAL', 'Check the scale cable and power'],
    NO_CLOCK: [SLOT_STOP, 'CLOCK NOT SET', 'Connect Wi-Fi to get the time'],
    FULL: [SLOT_STOP, 'MEMORY FULL', 'Send saved weighings to the Hub'],
    BLANK: [SLOT_STOP, 'OUT OF RANGE', 'Weight is past the scale range'],
    BELOW_MIN: [SLOT_STOP, 'BELOW MIN', 'Too light while Approval is ON'],
    OUT_OF_LIMITS: [SLOT_WAIT, 'OUTSIDE HI-LO LIMITS', 'Tap SAVE twice to save anyway'],
    ALARM: [SLOT_STOP, 'SCALE ALARM', 'The junction box shows an alarm'],
  };
  const RESULT = { ALREADY: 'ALREADY SAVED', NEED_ZERO: 'TAKE LOAD OFF', NOT_STABLE: 'WAIT: MOVING', EMPTY: 'SCALE IS EMPTY',
    NO_SIGNAL: 'NO SCALE SIGNAL', OUT_OF_LIMITS: 'TAP AGAIN: SAVE', WRITER: 'WRITER: NO SAVE', ALARM: 'SCALE ALARM' };
  const REASON = { motion: 'MOTION', empty: 'EMPTY', nosignal: 'NO SIGNAL', alarm: 'ALARM', noclock: 'NO CLOCK' };
  const HUB = { online: 'HUB', offline: 'HUB OFFLINE', nowifi: 'HUB', clock: 'HUB CLOCK', rejected: 'HUB REJECTED', setup: 'HUB SETUP' };
  const fmtW = (g, dec) => (g / 1000).toFixed(dec);
  const kgText = (g) => String(+(g / 1000).toFixed(3));

  // The Hi-Lo zone, by the relay rule (wsoft_relay_want): a live, checked weight above zero.
  function zoneOf(S) {
    const g = S.gross - S.tare;
    if (!S.linked || S.alarm || S.hold === 'blank' || !S.product || g <= 0) return ZONE_NONE;
    return g < S.product.low ? ZONE_LOW : g > S.product.high ? ZONE_HIGH : ZONE_OK;
  }
  // Where the marker sits on the bar, 0..1000 (the Edge's band: 0.6 of the span past each limit).
  function pointerOf(g, low, high) {
    const span = high - low;
    if (span <= 0) return g < low ? 0 : g > high ? 1000 : 500;
    const num = (g - low) * 10 + 6 * span, den = 22 * span;
    if (num <= 0) return 0;
    if (num >= den) return 1000;
    return Math.floor((1000 * num + den / 2) / den);
  }

  function modelOf(S) {
    const m = { dark: S.dark, page: { live: PAGE_LIVE, menu: PAGE_MENU, pin: PAGE_PIN, nopin: PAGE_NO_PIN, wifi: PAGE_WIFI }[S.page], style: S.style };
    const up = S.wifi.up, hub = up ? S.hub : 'nowifi', net = S.gross - S.tare, pr = S.product;
    m.wifi_up = up; m.wifi_bars = up ? S.wifi.bars : 0; m.ip = up ? S.wifi.ip : 'NO WI-FI';
    m.hub = HUB[hub]; m.hub_tone = hub === 'online' ? TONE_OK : hub === 'setup' ? TONE_SOFT : TONE_BAD;
    m.link = S.source === 'LOADCELL' ? 'CELL' : S.source; m.link_tone = !S.linked ? TONE_BAD : S.alarm ? TONE_WAIT : TONE_OK;
    // The band above the weight: a red note first, then the Wi-Fi alert, then a yellow note.
    const n = NOTICES[S.waiting];
    m.slot = SLOT_NONE; m.slot_title = ''; m.slot_detail = '';
    if (n && n[0] === SLOT_STOP) { m.slot = SLOT_STOP; m.slot_title = n[1]; m.slot_detail = n[2]; }
    else if (!up && !S.wifi.dismissed) m.slot = SLOT_WIFI;
    else if (n) { m.slot = SLOT_WAIT; m.slot_title = n[1]; m.slot_detail = n[2]; }
    m.trade_ok = S.check === 'ok';
    m.reason = !S.approval ? '' : m.trade_ok ? 'SEQ ' + S.seq : REASON[S.check] || 'CHECK';
    m.reason_tone = !S.approval ? TONE_SOFT : m.trade_ok ? TONE_OK : TONE_WAIT;
    m.steady = !S.linked ? 'NO SIGNAL' : S.alarm ? 'ALARM' : S.stable ? 'STABLE' : 'UNSTABLE';
    m.steady_tone = !S.linked || S.alarm ? TONE_BAD : S.stable ? TONE_OK : TONE_WAIT;
    m.plate_sa = S.plate ? `SA ${S.plate.sa}` : '';
    m.plate_limits = S.plate ? `MAX ${S.plate.max} MIN ${S.plate.min} E ${S.plate.e} KG` : '';
    m.audit = 'AUDIT ' + S.audit;
    m.label = S.tare ? 'NET (TARE)' : 'NET';
    m.live = S.linked;
    m.weight = S.linked && S.hold !== 'blank' ? fmtW(net, S.decimals) : '----';
    m.unit = S.unit;
    m.zone = zoneOf(S);
    m.product_on = !!pr;
    if (m.zone === ZONE_LOW) { m.word = 'TOO LOW'; m.word_tone = TONE_WAIT; }
    else if (m.zone === ZONE_OK) { m.word = 'OK'; m.word_tone = TONE_OK; }
    else if (m.zone === ZONE_HIGH) { m.word = 'TOO HIGH'; m.word_tone = TONE_BAD; }
    else if (!S.linked) { m.word = 'WAITING'; m.word_tone = TONE_SOFT; }
    else if (S.alarm) { m.word = 'ALARM'; m.word_tone = TONE_BAD; }
    else if (!pr) { m.word = 'NO PRODUCT'; m.word_tone = TONE_SOFT; }
    else if (S.hold === 'blank') { m.word = 'OUT OF RANGE'; m.word_tone = TONE_BAD; }
    else { m.word = 'EMPTY'; m.word_tone = TONE_SOFT; }
    m.product = pr ? pr.name.toUpperCase() : '';
    m.target = pr ? 'TARGET ' + kgText(pr.target) + ' KG' : S.products.length ? 'PICK ONE IN THE MENU' : 'SET LIMITS ON THE HUB';
    m.low = pr ? kgText(pr.low) : ''; m.high = pr ? kgText(pr.high) : '';
    m.pointer = pr && S.linked && !S.alarm && S.hold !== 'blank' ? pointerOf(net, pr.low, pr.high) : -1;
    m.gross = S.linked ? fmtW(S.gross, S.decimals) + ' ' + S.unit : '----';
    const r = S.save.recent;
    const writer = S.source === 'WRITER';
    if (writer && r && r.result === 'SAVED') { m.save_line = 'SAVED'; m.save_look = LOOK_ON; }
    else if (r && r.result === 'SAVED') { m.save_line = 'SAVED #' + r.seq; m.save_look = LOOK_ON; }
    else if (r) { m.save_line = RESULT[r.result]; m.save_look = r.result === 'ALREADY' || r.result === 'NEED_ZERO' ? LOOK_BTN : LOOK_BAD; }
    else if (S.waiting === 'ALREADY') { m.save_line = 'TAKE LOAD OFF'; m.save_look = LOOK_BTN; }
    else if (!writer && S.save.pending > 0) { m.save_line = S.save.pending + ' TO SEND'; m.save_look = LOOK_ON; }
    else { m.save_line = !writer && S.save.auto ? 'AUTO SAVE ON' : 'TAP TO SAVE'; m.save_look = LOOK_ON; }
    m.zero_btn = S.source === 'LOADCELL' && !S.tare;
    m.zero_line = m.zero_btn ? S.zeroNote || 'TAP: SET TO 0' : '';
    m.title = ['', 'MENU', 'ENTER PIN', 'APPROVAL', 'WI-FI'][m.page];
    m.message = S.message;
    m.tiles = [];
    const tile = (id, caption, value, look) => m.tiles.push({ id, caption, value, look });
    tile('approval', 'APPROVAL', S.approval ? 'ON' : 'OFF', S.approval ? LOOK_ON : LOOK_BTN);
    tile('auto', 'AUTO SAVE', S.save.auto ? 'ON' : 'OFF', S.save.auto ? LOOK_ON : LOOK_BTN);
    if (S.source === 'RS485' || S.source === 'LOADCELL') {
      tile('tare', 'TARE', S.tare ? fmtW(S.tare, S.decimals) + ' ' + S.unit : 'OFF', S.tare ? LOOK_ON : LOOK_BTN);
      if (S.presetTare) tile('preset', 'PRESET TARE', fmtW(S.presetTare, S.decimals) + ' ' + S.unit, LOOK_BTN);
    }
    if (S.source === 'LOADCELL') tile('zero', 'ZERO', 'SET TO 0', LOOK_BTN);
    tile('product', 'PRODUCT', !S.products.length ? 'NONE ON THE HUB' : pr ? pr.name.toUpperCase() : 'NONE', pr ? LOOK_ON : LOOK_BTN);
    tile('theme', 'SCREEN', S.style === STYLE_COLOUR ? 'COLOUR' : S.style === STYLE_DIAL ? 'DIAL' : S.dark ? 'DARK' : 'LIGHT', LOOK_BTN);
    tile('wifi', 'WI-FI', up ? S.wifi.ip : 'OFF', LOOK_BTN);
    m.tile_count = m.tiles.length;
    m.pin_count = S.pin.length;
    m.address = m.page === PAGE_NO_PIN && up ? S.wifi.ip : S.wifi.hotspot ? S.wifi.apIp : '';
    m.ap_name = S.wifi.hotspot ? S.wifi.ap : '';
    return m;
  }

  // ---------- the test screens ----------
  const MAIZE = { name: 'Maize', low: 49000, target: 50000, high: 51000 };
  const SUGAR = { name: 'Sugar 25', low: 24900, target: 25000, high: 25250 };
  const LONG = { name: 'Yellow maize meal 12.5', low: 12400, target: 12500, high: 12650 };
  function state(patch = {}) {
    const S = {
      dark: true, style: STYLE_CLASSIC, page: 'live', message: '', pin: '',
      wifi: { up: true, ip: '192.168.0.142', bars: 3, ap: 'Weighsoft-Esp-NEW', apIp: '192.168.4.1', hotspot: true, dismissed: false },
      hub: 'online', source: 'RS232', linked: true, alarm: false, stable: true,
      gross: 50120, tare: 0, presetTare: 0, decimals: 3, unit: 'kg', hold: 'none',
      zeroNote: '', approval: false, check: 'off', seq: 0, audit: 0, plate: null,
      products: [MAIZE, SUGAR], product: MAIZE,
      save: { auto: true, pending: 0, recent: null }, waiting: 'NONE', nextSeq: 8,
    };
    for (const [k, v] of Object.entries(patch)) {
      S[k] = v && typeof v === 'object' && !Array.isArray(v) && S[k] && k !== 'product' && k !== 'plate' ? { ...S[k], ...v } : v;
    }
    return S;
  }
  // title, one line for a person, the state
  const PRESETS = [
    ['Weighing, inside the limits', 'The weight panel is green and says OK. The marker sits in the green part of the bar.', state()],
    ['Too low', 'The panel turns yellow, like the LOW relay. The yellow note says how to save anyway.', state({ gross: 47350, waiting: 'OUT_OF_LIMITS' })],
    ['Too high', 'The panel turns red, like the HIGH relay.', state({ gross: 52480, waiting: 'OUT_OF_LIMITS' })],
    ['Light mode', 'The same screen in light. One tap in the Menu switches it.', state({ dark: false })],
    ['Saved', 'SAVE shows the number of the saved weighing.', state({ save: { recent: { result: 'SAVED', seq: 8 } }, waiting: 'ALREADY' })],
    ['Nothing on the scale', 'No colour until a load is on, the same as the relays.', state({ gross: 0, save: { auto: false } })],
    ['Wi-Fi drops', 'A yellow alert with CONNECT and X. The top bar shows NO WI-FI in red.', state({ wifi: { up: false } })],
    ['CONNECT tapped', 'The steps to put the board on Wi-Fi from a phone. It keeps looking for its saved Wi-Fi by itself.', state({ page: 'wifi', wifi: { up: false } })],
    ['Scale unplugged', 'A red note says what to check. The weight shows dashes, never an old number.', state({ linked: false, waiting: 'NO_SIGNAL' })],
    ['Approval on, with tare', 'Trade checks, the plate line, the audit counter and NET (TARE), in their old words.', state({ source: 'RS485', approval: true, check: 'ok', seq: 41, audit: 7, decimals: 2, gross: 52600, tare: 2500, presetTare: 2500, plate: { sa: '1234/56', max: '150', min: '1', e: '0.05' } })],
    ['No product picked', 'Plain weighing. The bar goes grey and says where to pick a product.', state({ product: null })],
    ['Menu', 'Each button shows what it is set to now. Green means on.', state({ page: 'menu', source: 'RS485', presetTare: 2500, decimals: 2, message: 'AUTO SAVE ON' })],
    ['PIN pad for Approval', 'Numbers laid out like a phone. Dots show how many you typed.', state({ page: 'pin', pin: '12' })],
    ['Approval tapped with no PIN', 'It says plainly where to set the PIN.', state({ page: 'nopin' })],
    ['Light mode, too high', 'The colours carry over to light mode.', state({ dark: false, gross: 52480, waiting: 'OUT_OF_LIMITS' })],
    ['Menu in light mode', 'The menu in light.', state({ dark: false, page: 'menu' })],
    // Long words must be cut to their room, never run into their neighbours.
    ['Long product name', 'The name is cut where the word begins.', state({ product: LONG, gross: 12700, waiting: 'OUT_OF_LIMITS' })],
    ['Long hotspot name', 'A long hotspot name drops to the smaller letters.', state({ page: 'wifi', wifi: { up: false, ap: 'esp-W-Soft/yellow2-workshop-bench' } })],
    ['Long menu message', 'The message stops before the title.', state({ page: 'menu', product: LONG, message: 'PRODUCT YELLOW MAIZE MEAL 12.5 KG BAG' })],
    ['Long plate line', 'A long SA number is cut and marked; Max, Min and e stay whole.', state({ source: 'RS485', approval: true, check: 'motion', stable: false, audit: 1234567, plate: { sa: 'NRCS/SA1234/2026-0001234567890', max: '60000', min: '400', e: '20' } })],
    ['Plate limits too long', 'If even the limits do not fit, whole words only, and dots say something is missing.', state({ source: 'RS485', approval: true, check: 'ok', seq: 5, audit: 4294967295, plate: { sa: '1', max: '99999.99999', min: '99999.99999', e: '99999.99999' } })],
    ['Wi-Fi page, connected', 'Once the board is on Wi-Fi the page shows its address.', state({ page: 'wifi' })],
    ['Wi-Fi page, hotspot off', 'No Wi-Fi and no hotspot: nothing to join.', state({ page: 'wifi', wifi: { up: false, hotspot: false } })],
    ['No PIN, no address', 'With neither Wi-Fi nor hotspot the note says to connect first.', state({ page: 'nopin', wifi: { up: false, hotspot: false } })],
    ['Longest status bar', 'The address is never cut: the Hub shows only its dot when the bar is full.', state({ source: 'WRITER', hub: 'rejected', wifi: { ip: '192.168.100.200', bars: 1 } })],
    ['Load cell', 'A board that weighs itself has ZERO next to MENU: empty the scale and tap.', state({ source: 'LOADCELL', gross: 1254, product: null, products: [] })],
    ['Load cell, ZERO refused', 'The button says why for a few seconds.', state({ source: 'LOADCELL', gross: 15, product: null, products: [], zeroNote: 'NOT STEADY', dark: false })],
    ['Load cell menu', 'ZERO is in the menu too, after TARE.', state({ page: 'menu', source: 'LOADCELL' })],
    // Full Colour: the whole screen is the Hi-Lo colour (MENU > SCREEN steps to it).
    ['Full Colour, inside the limits', 'The whole screen is green. The weight is as big as it fits.', state({ style: STYLE_COLOUR, source: 'LOADCELL' })],
    ['Full Colour, too low', 'The whole screen is yellow, with an arrow down.', state({ style: STYLE_COLOUR, source: 'LOADCELL', gross: 47350 })],
    ['Full Colour, too high', 'The whole screen is red, with an arrow up.', state({ style: STYLE_COLOUR, gross: 52480 })],
    ['Full Colour, a notice', 'A notice sits in a dark band over the colour.', state({ style: STYLE_COLOUR, gross: 47350, waiting: 'OUT_OF_LIMITS' })],
    ['Full Colour, take the load off', 'A red notice is the darkest band.', state({ style: STYLE_COLOUR, waiting: 'NEED_ZERO', save: { recent: { result: 'NEED_ZERO' } } })],
    ['Full Colour, Wi-Fi drops', 'CONNECT and X in the band.', state({ style: STYLE_COLOUR, wifi: { up: false } })],
    ['Full Colour, nothing on the scale', 'No colour until a load is on: the dark screen with the big weight.', state({ style: STYLE_COLOUR, gross: 0, save: { auto: false } })],
    ['Full Colour, Approval on with tare', 'The plate keeps its line; the product moves under the word.', state({ style: STYLE_COLOUR, source: 'RS485', approval: true, check: 'ok', seq: 41, audit: 7, decimals: 2, gross: 52600, tare: 2500, presetTare: 2500, plate: { sa: '1234/56', max: '150', min: '1', e: '0.05' } })],
    ['Full Colour, long product name', 'The name is cut to its room.', state({ style: STYLE_COLOUR, product: LONG, gross: 12700 })],
    ['Full Colour, menu', 'SCREEN says COLOUR.', state({ style: STYLE_COLOUR, page: 'menu' })],
    // Dial: the Hi-Lo bar as a gauge over the weight.
    ['Dial, inside the limits', 'The dot rides the arc. The word sits in a green pill.', state({ style: STYLE_DIAL, source: 'LOADCELL' })],
    ['Dial, too low', 'The dot is in the yellow part.', state({ style: STYLE_DIAL, source: 'LOADCELL', gross: 47350 })],
    ['Dial, too high', 'The dot is in the red part.', state({ style: STYLE_DIAL, gross: 52480 })],
    ['Dial, a notice', 'A notice covers the top of the arc.', state({ style: STYLE_DIAL, gross: 47350, waiting: 'OUT_OF_LIMITS' })],
    ['Dial, Wi-Fi drops', 'CONNECT and X over the arc.', state({ style: STYLE_DIAL, wifi: { up: false } })],
    ['Dial, no product picked', 'A grey arc, and where to pick a product.', state({ style: STYLE_DIAL, product: null })],
    ['Dial, scale unplugged', 'Dashes, never an old number.', state({ style: STYLE_DIAL, linked: false, waiting: 'NO_SIGNAL' })],
    ['Dial, Approval on with tare', 'The trade chip on two lines, the plate along the bottom.', state({ style: STYLE_DIAL, source: 'RS485', approval: true, check: 'ok', seq: 41, audit: 7, decimals: 2, gross: 52600, tare: 2500, presetTare: 2500, plate: { sa: '1234/56', max: '150', min: '1', e: '0.05' } })],
    ['Dial, long product name', 'The name is cut to its room.', state({ style: STYLE_DIAL, product: LONG, gross: 12700 })],
    ['Dial, six figures', 'A long weight drops to the smaller letters.', state({ style: STYLE_DIAL, gross: 982760, decimals: 0, product: { name: 'Maize', low: 980000, target: 982000, high: 984000 } })],
  ];

  const BOARDS = {
    yellow: { id: 'yellow', name: 'Yellow board', size: '2.8 inch, 320 x 240', W: 320, H: 240, sx: 1, sy: 1 },
    wave: { id: 'wave', name: 'Waveshare', size: '4.3 inch, 800 x 480', W: 800, H: 480, sx: 2.5, sy: 2 },
  };

  // CRC-32 (the zip one) of a frame, low byte of each dot first: what the host test checks.
  function crc32(fb) {
    let crc = 0xffffffff;
    const step = (byte) => { crc ^= byte; for (let k = 0; k < 8; k++) crc = crc & 1 ? (crc >>> 1) ^ 0xedb88320 : crc >>> 1; };
    for (let i = 0; i < fb.length; i++) { step(fb[i] & 255); step(fb[i] >> 8); }
    return (crc ^ 0xffffffff) >>> 0;
  }

  // Draws a frame on a browser canvas, each dot k x k.
  function show(canvas, c, k) {
    const out = new ImageData(c.W, c.H), d = out.data;
    for (let i = 0; i < c.fb.length; i++) {
      const v = c.fb[i], r = v >> 11, g = (v >> 5) & 63, b = v & 31;
      d[i * 4] = (r << 3) | (r >> 2); d[i * 4 + 1] = (g << 2) | (g >> 4); d[i * 4 + 2] = (b << 3) | (b >> 2); d[i * 4 + 3] = 255;
    }
    const tmp = document.createElement('canvas');
    tmp.width = c.W; tmp.height = c.H;
    tmp.getContext('2d').putImageData(out, 0, 0);
    canvas.width = c.W * k; canvas.height = c.H * k;
    const g = canvas.getContext('2d');
    g.imageSmoothingEnabled = false;
    g.drawImage(tmp, 0, 0, c.W * k, c.H * k);
  }

  return { ROLES, BOARDS, PRESETS, THEME_HEX, BAR_HEX, THEME, BAR, BOX, ROWS, tileBox, keyBox, Canvas, buildFonts, packFont, unpackFont,
    paint, paintRow, modelOf, state, zoneOf, crc32, show, fmtW,
    PAGE_LIVE, PAGE_MENU, PAGE_PIN, PAGE_NO_PIN, PAGE_WIFI, SLOT_NONE, SLOT_STOP, SLOT_WAIT, SLOT_WIFI, ZONE_NONE };
})();
