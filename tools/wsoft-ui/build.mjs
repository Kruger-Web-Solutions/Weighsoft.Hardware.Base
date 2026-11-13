// Builds the mock-up page: tools/wsoft-ui/out/mockup.html (one file, the reference drawing
// inlined) and out/preview.html (the same, wrapped so a browser opens it from disk).
// With fonts.json present (written by gen.mjs) the page carries the board's own letters
// and needs no web fonts; without it the browser draws them.
//   node tools/wsoft-ui/build.mjs
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const out = join(here, 'out');
mkdirSync(out, { recursive: true });

export function buildMockup() {
  const ref = readFileSync(join(here, 'ref.js'), 'utf8');
  const fontsFile = join(here, 'fonts.json');
  const fonts = existsSync(fontsFile) ? readFileSync(fontsFile, 'utf8') : 'null';
  let page = readFileSync(join(here, 'mockup.html'), 'utf8');
  const status = process.env.WSOFT_UI_STATUS || 'This screen is being built for both boards.';
  // The pictures the two bench boards sent of their own screens, when they are in the repo.
  const shots = join(here, '..', '..', 'docs', 'img', 'wsoft-screen');
  const uri = (name) => 'data:image/png;base64,' + readFileSync(join(shots, name)).toString('base64');
  const real = existsSync(join(shots, 'board-yellow-live.png')) && existsSync(join(shots, 'board-waveshare-live.png'))
    ? JSON.stringify({ yellow: uri('board-yellow-live.png'), wave: uri('board-waveshare-live.png') }) : 'null';
  page = page.replace('/*@ref*/', () => ref).replace('/*@fonts*/null', () => fonts).replace('/*@status*/', () => status)
    .replace('/*@real*/null', () => real);
  writeFileSync(join(out, 'mockup.html'), page);
  writeFileSync(join(out, 'preview.html'),
    `<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"></head><body style="margin:0">${page}</body></html>`);
  return join(out, 'mockup.html');
}

if (process.argv[1] === fileURLToPath(import.meta.url)) console.log(buildMockup());
