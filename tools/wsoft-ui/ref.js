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
  ];
  const TINY = 0, SMALL = 1, MID = 2, BIG = 3, HERO = 4;
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
    const ref = font.glyphs[(roleIndex === HERO ? 48 : 72) - role.first];
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
      for (let py = Math.max(Y0, 0); py < Math.min(Y1, this.H); py++) {
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
      for (let py = Math.max(Math.floor(Y - R - 1), 0); py <= Math.min(Math.floor(Y + R + 1), this.H - 1); py++) {
        for (let px = Math.max(Math.floor(X - R - 1), 0); px <= Math.min(Math.floor(X + R + 1), this.W - 1); px++) {
          const dx = px + 0.5 - X, dy = py + 0.5 - Y, cov = R - Math.sqrt(dx * dx + dy * dy) + 0.5;
          if (cov <= 0) continue;
          this.put(px, py, c, cov < 1 ? rnd(cov * 255) : 255);
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
      return w > f.track ? w - f.track : w;
    }
    width(str, role) { return this.dots(str, role) / this.sx; }
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
          if (py < 0 || py >= this.H) continue;
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
  const ZONE_NONE = 0, ZONE_LOW = 1, ZONE_OK = 2, ZONE_HIGH = 3;
  const LOOK_BTN = 0, LOOK_ON = 1, LOOK_ACCENT = 2, LOOK_PLAIN = 3, LOOK_BAD = 4;

  // ---------- where things are (layout units) ----------
  // Tappable boxes: x, y, w, h. The firmware uses the same numbers for its taps.
  const BOX = {
    wifiBar: [0, 0, 150, 20], slot: [6, 22, 308, 40], connect: [160, 26, 100, 32], dismiss: [266, 26, 42, 32],
    gross: [8, 190, 104, 46], menu: [120, 190, 72, 46], save: [200, 190, 112, 46],
    back: [164, 190, 148, 44], wide: [8, 190, 304, 44], left: [8, 190, 148, 44],
  };
  const tileBox = (i) => [i % 2 ? 164 : 8, 46 + (i >> 1) * 48, 148, 44];
  const KEY_X = [8, 86, 164, 242];
  const keyBox = (col, row, cols, rows) => [KEY_X[col], 46 + row * 48, 72 * cols + 6 * (cols - 1), 44 * rows + 4 * (rows - 1)];

  const tone = (T, t) => (t === TONE_OK ? T.ok_ink : t === TONE_WAIT ? T.wait_ink : t === TONE_BAD ? T.bad_ink : T.soft);
  const dot = (T, t) => (t === TONE_OK ? T.ok : t === TONE_WAIT ? T.wait : t === TONE_BAD ? T.bad : T.line);
  const barTone = (t) => (t === TONE_OK ? BAR.ok : t === TONE_WAIT ? BAR.warn : t === TONE_BAD ? BAR.bad : BAR.mute);

  // ---------- the live page, row by row ----------
  function paintBar(c, m) {
    c.rect(0, 0, 320, 20, BAR.bg, 0);
    c.rect(0, 19, 320, 1, BAR.line, 0);
    for (let i = 0; i < 4; i++) {
      const h = 4 + i * 3;
      c.rect(8 + i * 5, 15 - h, 3, h, m.wifi_up && i < m.wifi_bars ? BAR.ok : BAR.dim, 0);
    }
    c.text(m.ip, 32, 10, SMALL, m.wifi_up ? BAR.ink : BAR.bad, LEFT);
    let x = 312;
    x -= c.text(m.link, x, 10, SMALL, m.link_tone === TONE_OK ? BAR.ink : barTone(m.link_tone), RIGHT);
    c.disc(x - 7, 10, 3.5, barTone(m.link_tone));
    x -= 20;
    x -= c.text(m.hub, x, 10, SMALL, m.hub_tone === TONE_OK ? BAR.ink : barTone(m.hub_tone), RIGHT);
    c.disc(x - 7, 10, 3.5, barTone(m.hub_tone));
  }

  function paintTop(c, m, T) {
    c.rect(0, 20, 320, 42, T.bg, 0);
    if (m.slot === SLOT_WIFI) {
      c.rect(6, 22, 308, 40, T.wait, 8);
      c.text('WI-FI IS OFF', 18, 42, MID, DARK, LEFT);
      c.rect(160, 26, 100, 32, DARK, 6);
      c.text('CONNECT', 210, 42, SMALL, WHITE, CENTER);
      c.rect(266, 26, 42, 32, DARK, 6);
      c.rect(267.5, 27.5, 39, 29, T.wait, 5);
      c.text('X', 287, 42, MID, DARK, CENTER);
      return;
    }
    if (m.slot !== SLOT_NONE) {
      const stop = m.slot === SLOT_STOP, ink = stop ? WHITE : DARK;
      c.rect(6, 22, 308, 40, stop ? T.stop : T.wait, 8);
      c.text(m.slot_title, 160, 34, c.width(m.slot_title, BIG) <= 292 ? BIG : MID, ink, CENTER);
      c.text(m.slot_detail, 160, 53, SMALL, ink, CENTER);
      return;
    }
    const chip = m.trade_ok ? 'TRADE CHECKS ON' : 'NOT FOR TRADE';
    const w = c.width(chip, SMALL) + 14;
    c.rect(8, 24, w, 17, m.trade_ok ? T.ok : T.bad, 4);
    c.rect(9, 25, w - 2, 15, m.trade_ok ? T.chip_ok : T.chip_bad, 3);
    c.text(chip, 8 + w / 2, 32.5, SMALL, m.trade_ok ? T.ok_ink : T.bad_ink, CENTER);
    c.text(m.reason, 8 + w + 7, 32.5, TINY, tone(T, m.reason_tone), LEFT);
    const sw = c.text(m.steady, 312, 32.5, SMALL, tone(T, m.steady_tone), RIGHT);
    c.disc(312 - sw - 8, 32.5, 4, dot(T, m.steady_tone));
    c.text(m.plate, 10, 52, TINY, T.soft, LEFT);
    c.text(m.audit, 312, 52, TINY, T.soft, RIGHT);
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
    c.text(m.word, 28, 137, m.product_on ? BIG : MID, tone(T, m.word_tone), LEFT);
    const bx = 10, bw = 300, by = 157, bh = 12;
    if (!m.product_on) {
      c.text(m.target, 310, 137, TINY, T.soft, RIGHT);
      c.rect(bx, by, bw, bh, T.line, -1);
      return;
    }
    c.text(m.product, 310, 129.5, SMALL, T.ink, RIGHT);
    c.text(m.target, 310, 143, TINY, T.soft, RIGHT);
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
    c.rect(8, 190, 104, 46, T.line, 8);
    c.rect(9, 191, 102, 44, T.panel, 7);
    c.text('GROSS', 17, 202, TINY, T.soft, LEFT);
    c.text(m.gross, 17, 221, c.width(m.gross, MID) <= 88 ? MID : SMALL, m.live ? T.ink : T.soft, LEFT);
    c.rect(120, 190, 72, 46, T.btn, 8);
    for (let i = 0; i < 3; i++) c.rect(144, 200 + i * 6, 24, 3, T.btn_ink, 1);
    c.text('MENU', 156, 226, SMALL, T.btn_ink, CENTER);
    c.rect(200, 190, 112, 46, m.save_look === LOOK_ON ? T.ok : m.save_look === LOOK_BAD ? T.stop : T.btn, 8);
    const ink = m.save_look === LOOK_BTN ? T.btn_ink : WHITE;
    c.text('SAVE', 256, 206, MID, ink, CENTER);
    c.text(m.save_line, 256, 225, c.width(m.save_line, SMALL) <= 104 ? SMALL : TINY, ink, CENTER);
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
      c.text(value, x + 10, y + 30, c.width(value, MID) <= w - 20 ? MID : SMALL, ink, LEFT);
    } else {
      c.text(value, x + w / 2, y + h / 2, MID, ink, CENTER);
    }
  }

  function paintPage(c, m, T) {
    c.rect(0, 20, 320, 220, T.bg, 0);
    c.text(m.title, 10, 32, SMALL, T.ink, LEFT);
    c.text(m.message, 310, 32, SMALL, T.wait_ink, RIGHT);
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
      c.text('On a phone or PC, open', 160, 90, SMALL, T.soft, CENTER);
      c.text(m.address, 160, 112, MID, T.ink, CENTER);
      c.text('Log in as admin, then', 160, 140, SMALL, T.soft, CENTER);
      c.text('W-Soft  >  Settings  >  Approval PIN', 160, 160, SMALL, T.ink, CENTER);
      paintTile(c, T, BOX.wide, '', 'BACK', LOOK_PLAIN);
    } else if (m.wifi_up) {
      c.text('CONNECTED', 160, 84, BIG, T.ok_ink, CENTER);
      c.text(m.ip, 160, 118, MID, T.ink, CENTER);
      c.text('Type this address in a browser on the same Wi-Fi', 160, 144, TINY, T.soft, CENTER);
      paintTile(c, T, BOX.wide, '', 'BACK', LOOK_PLAIN);
    } else {
      for (let i = 0; i < 3; i++) {
        c.disc(22, 58 + i * 40, 10, T.accent);
        c.text(String.fromCharCode(49 + i), 22, 58 + i * 40, SMALL, WHITE, CENTER);
      }
      c.text('On your phone, join this Wi-Fi', 42, 50, TINY, T.soft, LEFT);
      c.text(m.ap_name, 42, 67, MID, T.ink, LEFT);
      c.text('Then open this address', 42, 90, TINY, T.soft, LEFT);
      c.text(m.address, 42, 107, MID, T.ink, LEFT);
      c.text('Log in, tap Wi-Fi and pick yours', 42, 138, SMALL, T.ink, LEFT);
      c.text(m.wifi_trying ? 'Trying your saved Wi-Fi again...' : 'Or tap TRY AGAIN to use the saved Wi-Fi', 42, 170, TINY, m.wifi_trying ? T.wait_ink : T.soft, LEFT);
      paintTile(c, T, BOX.left, '', 'TRY AGAIN', LOOK_ACCENT);
      paintTile(c, T, BOX.back, '', 'BACK', LOOK_PLAIN);
    }
  }

  // The rows of the screen: first and last layout line, and what draws them.
  const ROW_BAR = 0, ROW_TOP = 1, ROW_WEIGHT = 2, ROW_HILO = 3, ROW_BOTTOM = 4, ROW_PAGE = 5;
  const ROWS = [[0, 20], [20, 62], [62, 122], [122, 188], [188, 240], [20, 240]];
  function paintRow(c, m, row) {
    const T = m.dark ? THEME.dark : THEME.light;
    if (row === ROW_BAR) paintBar(c, m);
    else if (row === ROW_TOP) paintTop(c, m, T);
    else if (row === ROW_WEIGHT) paintWeight(c, m, T);
    else if (row === ROW_HILO) paintHilo(c, m, T);
    else if (row === ROW_BOTTOM) paintBottom(c, m, T);
    else paintPage(c, m, T);
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
  };
  const RESULT = { ALREADY: 'ALREADY SAVED', NEED_ZERO: 'TAKE LOAD OFF', NOT_STABLE: 'WAIT: MOVING', EMPTY: 'SCALE IS EMPTY',
    NO_SIGNAL: 'NO SCALE SIGNAL', OUT_OF_LIMITS: 'TAP AGAIN: SAVE', WRITER: 'WRITER: NO SAVE' };
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
    const m = { dark: S.dark, page: { live: PAGE_LIVE, menu: PAGE_MENU, pin: PAGE_PIN, nopin: PAGE_NO_PIN, wifi: PAGE_WIFI }[S.page] };
    const up = S.wifi.up, hub = up ? S.hub : 'nowifi', net = S.gross - S.tare, pr = S.product;
    m.wifi_up = up; m.wifi_bars = up ? S.wifi.bars : 0; m.ip = up ? S.wifi.ip : 'NO WI-FI';
    m.hub = HUB[hub]; m.hub_tone = hub === 'online' ? TONE_OK : hub === 'setup' ? TONE_SOFT : TONE_BAD;
    m.link = S.source; m.link_tone = !S.linked ? TONE_BAD : S.alarm ? TONE_WAIT : TONE_OK;
    // The band above the weight: a red note first, then the Wi-Fi alert, then a yellow note.
    const n = NOTICES[S.waiting];
    m.slot = SLOT_NONE; m.slot_title = ''; m.slot_detail = '';
    if (n && n[0] === SLOT_STOP) { m.slot = SLOT_STOP; m.slot_title = n[1]; m.slot_detail = n[2]; }
    else if (!up && !S.wifi.dismissed) m.slot = SLOT_WIFI;
    else if (n) { m.slot = SLOT_WAIT; m.slot_title = n[1]; m.slot_detail = n[2]; }
    m.trade_ok = S.check === 'ok';
    m.reason = !S.approval ? 'APPROVAL OFF' : m.trade_ok ? 'SEQ ' + S.seq : REASON[S.check] || 'CHECK';
    m.reason_tone = !S.approval ? TONE_SOFT : m.trade_ok ? TONE_OK : TONE_WAIT;
    m.steady = !S.linked ? 'NO SIGNAL' : S.alarm ? 'ALARM' : S.stable ? 'STABLE' : 'UNSTABLE';
    m.steady_tone = !S.linked || S.alarm ? TONE_BAD : S.stable ? TONE_OK : TONE_WAIT;
    m.plate = S.plate ? `SA ${S.plate.sa}   MAX ${S.plate.max} MIN ${S.plate.min} E ${S.plate.e} KG` : '';
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
    if (S.source === 'WRITER') { m.save_line = 'WRITER: NO SAVE'; m.save_look = LOOK_BTN; }
    else if (r && r.result === 'SAVED') { m.save_line = 'SAVED #' + r.seq; m.save_look = LOOK_ON; }
    else if (r) { m.save_line = RESULT[r.result]; m.save_look = r.result === 'ALREADY' || r.result === 'NEED_ZERO' ? LOOK_BTN : LOOK_BAD; }
    else if (S.waiting === 'ALREADY') { m.save_line = 'TAKE LOAD OFF'; m.save_look = LOOK_BTN; }
    else if (S.save.pending > 0) { m.save_line = S.save.pending + ' TO SEND'; m.save_look = LOOK_ON; }
    else { m.save_line = S.save.auto ? 'AUTO SAVE ON' : 'TAP TO SAVE'; m.save_look = LOOK_ON; }
    m.title = ['', 'MENU', 'ENTER PIN', 'APPROVAL', 'WI-FI'][m.page];
    m.message = S.message;
    m.tiles = [];
    const tile = (id, caption, value, look) => m.tiles.push({ id, caption, value, look });
    tile('approval', 'APPROVAL', S.approval ? 'ON' : 'OFF', S.approval ? LOOK_ON : LOOK_BTN);
    tile('auto', 'AUTO SAVE', S.save.auto ? 'ON' : 'OFF', S.save.auto ? LOOK_ON : LOOK_BTN);
    if (S.source === 'RS485') {
      tile('tare', 'TARE', S.tare ? fmtW(S.tare, S.decimals) + ' ' + S.unit : 'OFF', S.tare ? LOOK_ON : LOOK_BTN);
      if (S.presetTare) tile('preset', 'PRESET TARE', fmtW(S.presetTare, S.decimals) + ' ' + S.unit, LOOK_BTN);
    }
    tile('product', 'PRODUCT', !S.products.length ? 'NONE ON THE HUB' : pr ? pr.name.toUpperCase() : 'NONE', pr ? LOOK_ON : LOOK_BTN);
    tile('theme', 'SCREEN', S.dark ? 'DARK' : 'LIGHT', LOOK_BTN);
    tile('wifi', 'WI-FI', up ? S.wifi.ip : 'OFF', LOOK_BTN);
    m.tile_count = m.tiles.length;
    m.pin_count = S.pin.length;
    m.address = m.page === PAGE_NO_PIN && up ? S.wifi.ip : S.wifi.apIp;
    m.ap_name = S.wifi.ap;
    m.wifi_trying = S.wifi.trying;
    return m;
  }

  // ---------- the test screens ----------
  const MAIZE = { name: 'Maize', low: 49000, target: 50000, high: 51000 };
  const SUGAR = { name: 'Sugar 25', low: 24900, target: 25000, high: 25250 };
  function state(patch = {}) {
    const S = {
      dark: true, page: 'live', message: '', pin: '',
      wifi: { up: true, ip: '192.168.0.142', bars: 3, ap: 'Weighsoft-Esp-NEW', apIp: '192.168.4.1', dismissed: false, trying: false },
      hub: 'online', source: 'RS232', linked: true, alarm: false, stable: true,
      gross: 50120, tare: 0, presetTare: 0, decimals: 3, unit: 'kg', hold: 'none',
      approval: false, check: 'off', seq: 0, audit: 0, plate: null,
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
    ['CONNECT tapped', 'It tries your saved Wi-Fi again and shows how to set it from a phone.', state({ page: 'wifi', wifi: { up: false, trying: true } })],
    ['Scale unplugged', 'A red note says what to check. The weight shows dashes, never an old number.', state({ linked: false, waiting: 'NO_SIGNAL' })],
    ['Approval on, with tare', 'Trade checks, the plate line, the audit counter and NET (TARE), in their old words.', state({ source: 'RS485', approval: true, check: 'ok', seq: 41, audit: 7, decimals: 2, gross: 52600, tare: 2500, presetTare: 2500, plate: { sa: '1234/56', max: '150', min: '1', e: '0.05' } })],
    ['No product picked', 'Plain weighing. The bar goes grey and says where to pick a product.', state({ product: null })],
    ['Menu', 'Each button shows what it is set to now. Green means on.', state({ page: 'menu', source: 'RS485', presetTare: 2500, decimals: 2, message: 'AUTO SAVE ON' })],
    ['PIN pad for Approval', 'Numbers laid out like a phone. Dots show how many you typed.', state({ page: 'pin', pin: '12' })],
    ['Approval tapped with no PIN', 'It says plainly where to set the PIN.', state({ page: 'nopin' })],
    ['Light mode, too high', 'The colours carry over to light mode.', state({ dark: false, gross: 52480, waiting: 'OUT_OF_LIMITS' })],
    ['Menu in light mode', 'The menu in light.', state({ dark: false, page: 'menu' })],
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
