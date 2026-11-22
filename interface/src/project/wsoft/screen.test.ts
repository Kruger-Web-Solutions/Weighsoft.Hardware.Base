import { describe, expect, it } from '@jest/globals';
import { readFileSync } from 'fs';
import { join } from 'path';

import WsoftUi from './gen/wsoftScreen';
import { paintPicture, paintScreen, tapPoint, toRgba, SCREEN_H, SCREEN_W } from './screen';

const root = join(__dirname, '..', '..', '..', '..');

describe('the browser copy of the board screen', () => {
  it('is the reference drawing itself, not a second one', () => {
    const ref = readFileSync(join(root, 'tools/wsoft-ui/ref.js'), 'utf8');
    const copy = readFileSync(join(__dirname, 'gen/wsoftScreen.js'), 'utf8');
    expect(ref.length).toBeGreaterThan(30000);
    // Run "node tools/wsoft-ui/gen.mjs" after changing ref.js.
    expect(copy.includes(ref)).toBe(true);
  });

  it('paints every test screen dot for dot like the yellow board', () => {
    // The same checksums the firmware's host test holds the board's painting to.
    const golden = readFileSync(join(root, 'test/test_wsoft_ui/golden.h'), 'utf8');
    const row = /GOLDEN_1X\[\] = \{([^}]*)\}/.exec(golden);
    const want = (row ? row[1] : '').split(',').map((v) => parseInt(v.trim(), 16));
    expect(want.length).toBeGreaterThanOrEqual(28);
    expect(WsoftUi.PRESETS.length).toBe(want.length);
    WsoftUi.PRESETS.forEach(([title, , state], n) => {
      // Through JSON, as the words arrive from the board.
      const model = JSON.parse(JSON.stringify(WsoftUi.modelOf(state)));
      expect([title, WsoftUi.crc32(paintScreen(model))]).toEqual([title, want[n]]);
    });
  });

  it('paints the big picture dot for dot like the Waveshare', () => {
    const golden = readFileSync(join(root, 'test/test_wsoft_ui/golden.h'), 'utf8');
    const row = /GOLDEN_2X\[\] = \{([^}]*)\}/.exec(golden);
    const want = (row ? row[1] : '').split(',').map((v) => parseInt(v.trim(), 16));
    expect(WsoftUi.PRESETS.length).toBe(want.length);
    WsoftUi.PRESETS.forEach(([title, , state], n) => {
      const model = JSON.parse(JSON.stringify(WsoftUi.modelOf(state)));
      expect([title, WsoftUi.crc32(paintPicture(model))]).toEqual([title, want[n]]);
    });
  });

  it('turns the dots into picture bytes', () => {
    const rgba = toRgba(Uint16Array.from([0xffff, 0x0000, 0xf800, 0x07e0, 0x001f]));
    expect(Array.from(rgba)).toEqual([
      255, 255, 255, 255, 0, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255
    ]);
  });

  it('maps a click to the screen layout at any picture size', () => {
    const box = { left: 100, top: 50, width: 640, height: 480 };
    expect(tapPoint(100, 50, box)).toEqual({ x: 0, y: 0 });
    expect(tapPoint(739, 529, box)).toEqual({ x: SCREEN_W - 1, y: SCREEN_H - 1 });
    expect(tapPoint(420, 290, box)).toEqual({ x: 160, y: 120 });
    expect(tapPoint(99, 60, box)).toBeUndefined();
    expect(tapPoint(740, 60, box)).toBeUndefined();
    expect(tapPoint(200, 530, box)).toBeUndefined();
    expect(tapPoint(10, 10, { left: 0, top: 0, width: 0, height: 0 })).toBeUndefined();
  });
});
