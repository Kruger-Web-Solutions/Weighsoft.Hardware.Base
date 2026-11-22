// Types for wsoftScreen.js, which tools/wsoft-ui/gen.mjs writes from the reference drawing
// (tools/wsoft-ui/ref.js). Only what the browser copy of the screen uses.
export interface WsoftScreenFont {
  first: number;
  count: number;
  cap: number;
  track: number;
}
export interface WsoftScreenPacked extends WsoftScreenFont {
  glyphs: number[][];
  b64: string;
}
export interface WsoftScreenCanvas {
  W: number;
  H: number;
  fb: Uint16Array;
}
/** The words on the board's screen: GET /rest/wsoftScreen (WsoftUiModel in the firmware). */
export type WsoftScreenModel = Record<string, unknown> & { page: number };

declare const WsoftUi: {
  Canvas: new (W: number, H: number, sx: number, sy: number, fonts: WsoftScreenFont[]) => WsoftScreenCanvas;
  unpackFont: (packed: WsoftScreenPacked) => WsoftScreenFont;
  paint: (canvas: WsoftScreenCanvas, model: WsoftScreenModel) => void;
  crc32: (fb: Uint16Array) => number;
  modelOf: (state: unknown) => WsoftScreenModel;
  PRESETS: [string, string, unknown][];
};
/** The yellow board's letters (320 x 240) and the Waveshare's (800 x 480). */
export const FONTS: WsoftScreenPacked[];
export const FONTS_2X: WsoftScreenPacked[];
export default WsoftUi;
