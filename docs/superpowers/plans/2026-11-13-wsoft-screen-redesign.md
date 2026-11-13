# Plan: W-Soft board screen redesign

Spec: [2026-11-13-wsoft-screen-redesign-design.md](../specs/2026-11-13-wsoft-screen-redesign-design.md).
Branch `claude/wsoft-screen-redesign` off `claude/project-thread-9f4l7x` @ 45b2d49.
Worktree `C:\Projects\Weighsoft.Hardware.Base-screen-redesign`. One PR, medium size.

Rule for every code task: the test is written and seen failing before the code.
Host tests: `$env:PATH = "$env:USERPROFILE\.platformio\packages\toolchain-gccmingw32\bin;$env:PATH"`
then `C:\Python313\python.exe -m platformio test -e native -f test_wsoft_ui`.

## Tasks

| # | Task | Files | Done when |
|---|---|---|---|
| 0 | Full flash backup of both boards | `C:\Projects\firmware\backups\2026-11-13\` (not in git: holds Wi-Fi and Hub secrets) | Both `.bin` files have the full flash size |
| 1 | Reference drawing, mock-up, generator | `tools/wsoft-ui/ref.js`, `mockup.html`, `build.mjs`, `gen.html`, `gen.mjs`, `shoot.ps1` | `node tools/wsoft-ui/gen.mjs` writes the letters, colours, test screens, `golden.h` and the pictures |
| 2 | Drawing engine | `lib/wsoft_core/src/wsoft_gfx.h`, `.cpp`; `test/test_wsoft_ui/test_main.cpp` | Tests for blend, rect clip, rounded corner coverage, letters and CRC pass |
| 3 | Paint and model | `lib/wsoft_core/src/wsoft_ui_paint.h`, `.cpp` | All 16 test screens match `golden.h` on 320 x 240 and 800 x 480, whole-frame and in 16-line bands |
| 4 | Board side | `lib/wsoft_esp/src/wsoft_ui.h`, `.cpp`; `wsoft_screen.cpp` slimmed; `hub.cpp` (SCREEN command); `screen_rle.h`, `fonts.h` removed; `platformio.ini` (TFT_eSPI font flags off for `wsoft_cyd28`, SSE flags for `native`) | `wsoft_cyd28`, `wsoft_waveshare43` and `wsoft_s3_headless` build |
| 5 | Relay zone on the net weight | `lib/wsoft_esp/src/wsoft_screen.cpp` | Relay input uses the same net weight as the screen word |
| 6 | Yellow board: flash, check | COM13 (CH340, MAC 6c:c8:40:45:60:f4) | Boots; `SCREEN SHOT n` CRCs equal `golden.h`; live `SCREEN SHOT` decodes to the expected screen; `SCREEN TAP` opens MENU, switches SCREEN, opens WI-FI |
| 7 | Waveshare: flash, check | COM11 (CH343, MAC 94:a9:90:02:96:68; never the S3 44:1b:f6:fe:2a:7c). App only at 0x10000, so the working bootloader stays | Same checks |
| 8 | Review | Fresh reviewer on the diff | Critical and important findings fixed, tests green |
| 9 | Docs, PR | `docs/WSOFT-CYD28.md`, `docs/WSOFT-WAVESHARE43.md`, `docs/WSOFT-SCREEN.md` (letters licence, tools), `docs/superpowers/queue.md`, `work-log.jsonl` | Draft PR open against `claude/project-thread-9f4l7x` |

## Flash rules

- Never erase. Settings, PIN, Hub pairing and saved weighings stay.
- One build at a time: the web interface build writes into one shared folder.
- If a board does not come up right and the cause is not found in two tries, write its backup
  back (`esptool write_flash 0 <backup>.bin`, DIO mode on the Waveshare) and report.

## Status log

- 13 Nov 2026 22:45 SAST: task 0 done. Yellow: 4,194,304 bytes. Waveshare: 16,777,216 bytes.
- 13 Nov 2026 23:10 SAST: task 1 done. Letters: 27,631 bytes (yellow), 100,423 bytes (Waveshare).
