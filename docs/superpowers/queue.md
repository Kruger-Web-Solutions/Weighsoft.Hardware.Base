# Initiative Queue

| # | Initiative | Branch | Status | Plan | Notes |
|---|------------|--------|--------|------|-------|
| 1 | All-in-one ESP firmware: lift TFT weight screen + character-LCD onto the trunk as per-board compile-time options | `NewEspAllInOne` | IN PROGRESS | [plan](plans/2026-04-30-multi-mode-serial-phase-1.md), [display-integration](plans/2026-06-14-newespallinone-display-integration.md) | Off `SerialReaderWriter` trunk. U1 scaffold → U2 remote-weight receiver → U3 TFT (renamed `tftdisplay`) → U4 character-LCD (renamed `lcddisplay`). Serial execution, no worktrees. Sender multi-URL rewrite OUT of scope. |
