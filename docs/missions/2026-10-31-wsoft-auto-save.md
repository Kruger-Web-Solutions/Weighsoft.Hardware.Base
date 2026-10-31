# Mission: W-Soft auto-save, stable timer and on-screen SAVE button

STATUS: SUPERSEDED by docs/missions/2026-10-31-wsoft-save-weighing.md (same request, filed first by the coordinator with the Hub question already answered: saves go to the Hub via /api/v1/ingest/weighings). No work was dispatched from this brief.

## MISSION BRIEF

```text
MISSION BRIEF
OBJECTIVE:    Once a load on the W-Soft scale has been stable for an operator-set number of
              seconds, the weight is saved once (auto-save, switchable on/off in W-Soft >
              Settings), and the operator can also save it by tapping SAVE on the ESP's
              touchscreen; a second save of the same load does not happen.
CONSTRAINTS:  - Owner request (Jurien, 31 Oct 2026): toggle + stable timer on the settings
                screen; a SAVE button on the screen if the hardware has touch.
              - Hardware: Waveshare ESP32-S3-Touch-LCD-4.3, GT911 capacitive touch on I2C
                (SDA 8 / SCL 9), already initialised by ESP32_Display_Panel board->init().
              - Approval rules unchanged: approval switching stays USB-only; with approval ON
                a weight is a trade weight only when stable, sequenced and queued in flash;
                nothing is ever dropped (queue full = no new save).
              - Stability: today TRADE_STABLE_MS = 1000. The operator timer may lengthen it,
                never shorten it below 1 s.
              - Settings live in the framework's LittleFS JSON (wsoft_settings), edited on the
                W-Soft Settings page; bad values return 400 and keep the old ones.
              - Repo is PUBLIC: no secrets. C++ on the ESP, React UI. Host tests in
                lib/wsoft_core with test/test_*.
              - Nothing is flashed by agents; the owner flashes.
SUCCESS:      1. (OWNER) W-Soft > Settings has "Auto-save" on/off and "Stable for N seconds".
              2. (OWNER) With auto-save on, a load held still for N seconds is saved once;
                 taking it off (or changing it) re-arms the next save.
              3. (OWNER) The ESP screen shows a SAVE button; tapping it saves the current
                 weight once it is stable; it says why when it cannot (moving, empty, no signal).
              4. (DERIVED) Where an approval-OFF save goes follows the owner's A/B/C choice.
              5. (DERIVED) Host tests cover: timer edge (N-1 s no save, N s save), one save per
                 load, re-arm on empty/change, manual save while moving refused, settings range.
              6. (DERIVED) pio run -e wsoft_waveshare43 builds; all native suites pass.
ASSUMPTIONS:  | assumption | conf | blast radius if wrong |
              | "Save" means a record the Hub (or the ESP) keeps, not a print | M | medium: a printer path is different work |
              | Timer range 1-30 s, default 3 s | M | low: a number in settings |
              | GT911 reads work alongside the RGB panel without tearing the display | M | medium: touch may need its own polling cadence |
              | Hub accepts extra heartbeat fields without a schema change (option A needs a Hub change) | L | high for A: Scale Models Hub work |
MISSION:      weighsoft-op-feature
DEFERRED:     none
GATES:        Scope choice A/B/C (op-feature stage-2, asked). DEMO: required — the owner
              judges it on the bench screen and the Hub.
BUDGET:       standard
```

## Scope choice (asked to Jurien, 31 Oct 2026)

When approval is OFF, where does a saved weight go?
- A) To the Hub, marked "Not for trade" (recommended; needs a Hub change; ~3-4 h).
- B) Only the existing trade path: auto-save, timer and button work only with approval ON (~2 h).
- C) Kept on the ESP only, listed on the W-Soft page (~2-3 h).

## Ledger

| # | Agent | Objective | Status | SITREP |
|---|---|---|---|---|
