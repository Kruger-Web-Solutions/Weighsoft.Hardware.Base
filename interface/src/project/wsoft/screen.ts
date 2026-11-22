import WsoftUi, { FONTS, WsoftScreenFont, WsoftScreenModel } from './gen/wsoftScreen';

// The board's screen in the browser (Rhyno, 22 Nov 2026: one screen, two places). The board
// sends the words on its screen; this paints them with the board's own drawing and letters
// (gen/wsoftScreen.js, made from tools/wsoft-ui/ref.js), so the two cannot drift apart.

export const SCREEN_W = 320;
export const SCREEN_H = 240;

let fonts: WsoftScreenFont[] | undefined;

/** The screen for these words as 16-bit dots (5-6-5), SCREEN_W x SCREEN_H. */
export const paintScreen = (model: WsoftScreenModel): Uint16Array => {
  if (!fonts) fonts = FONTS.map(WsoftUi.unpackFont);
  const canvas = new WsoftUi.Canvas(SCREEN_W, SCREEN_H, 1, 1, fonts);
  WsoftUi.paint(canvas, model);
  return canvas.fb;
};

/** The dots as RGBA bytes for a browser canvas. */
export const toRgba = (fb: Uint16Array): Uint8ClampedArray => {
  const out = new Uint8ClampedArray(fb.length * 4);
  for (let i = 0; i < fb.length; i++) {
    const v = fb[i], r = v >> 11, g = (v >> 5) & 63, b = v & 31;
    out[i * 4] = (r << 3) | (r >> 2);
    out[i * 4 + 1] = (g << 2) | (g >> 4);
    out[i * 4 + 2] = (b << 3) | (b >> 2);
    out[i * 4 + 3] = 255;
  }
  return out;
};

/**
 * Where a click on the picture lands in the screen's own 320 x 240 layout, whatever size the
 * picture is shown at. Undefined when it is off the screen.
 */
export const tapPoint = (
  clickX: number, clickY: number, box: { left: number; top: number; width: number; height: number }
): { x: number; y: number } | undefined => {
  if (box.width <= 0 || box.height <= 0) return undefined;
  const x = Math.floor(((clickX - box.left) * SCREEN_W) / box.width);
  const y = Math.floor(((clickY - box.top) * SCREEN_H) / box.height);
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return undefined;
  return { x, y };
};
