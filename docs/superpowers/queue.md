# Initiative Queue

| # | Initiative | Branch | Status | Plan | Notes |
|---|------------|--------|--------|------|-------|
| 1 | All-in-one ESP firmware: lift TFT weight screen + character-LCD onto the trunk as per-board compile-time options | `NewEspAllInOne` | IN PROGRESS (2/4 units) | [plan](plans/2026-04-30-multi-mode-serial-phase-1.md), [display-integration](plans/2026-06-14-newespallinone-display-integration.md) | Off `SerialReaderWriter` trunk. ✅ U1 scaffold (a68e3b0). ✅ pre-existing S3 USB-CDC writer fix (87c2605). ✅ U2 remote-weight receiver behind FT_REMOTE_WEIGHT (7c16c0b) — nodemcu+esp32s3 green, tsc clean. ⬜ U3 TFT (rename `tftdisplay`, FT_TFT_WEIGHT_SCREEN, +esp32dev_tft/cyd_tft envs). ⬜ U4 character-LCD (rename `lcddisplay`, FT_DISPLAY_LCD). Serial execution, no worktrees. Sender multi-URL rewrite OUT of scope. |
