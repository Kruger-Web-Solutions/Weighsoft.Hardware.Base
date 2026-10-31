# Mission: W-Soft saves a weighing (auto-save, touch SAVE, web Save)

STATUS: DONE (bench, test mode)

## MISSION BRIEF

```text
MISSION BRIEF
OBJECTIVE:    On the Waveshare W-Soft ESP, an operator can keep a weighing: it saves itself once
              per load after the reading has been steady for a user-set number of seconds
              (switchable on/off in W-Soft settings), or on a tap of SAVE on the touchscreen or
              the web page; each saved weighing survives a power cut and reaches the paired Hub.
CONSTRAINTS:  - Jurien 31 Oct 12:08 UTC: auto-save on/off + stable-time setting on the same
                settings screen; touch SAVE button on the screen (hardware: Waveshare
                ESP32-S3-Touch-LCD-4.3, GT911 touch per the board definition in use).
              - Build on the existing firmware (Henzard/Rhyno 25-26 Oct): no rewrite; reuse
                TradeEngine stability rules, approval.cpp queue pattern and hub.cpp signing.
              - Approval ON: trade rules still govern (stable, not empty, clock valid, queue
                not full; never drop a queued weight). Approval OFF: marked not-for-trade.
              - A failed or stale poll is never a weight; DJB alarm blocks a save.
              - One save per load: after a save, re-arm only when the scale returns to empty.
              - Hub contract is fixed: signed POST /api/v1/ingest/weighings, weighing_id
                idempotent. Hub PR #78 (merged + deployed 31 Oct) accepts decimal kg, up to
                5 digits . 5 decimals, stored exactly: send the exact reading as decimal kg.
                Do not change the Hub here.
              - Never invent protocol, pinout or calibration. Touch pins/driver come from the
                ESP32_Display_Panel board config already used; if touch is not in that config,
                stop and report.
              - No Python in product code. No secrets in commits. Office passwords left as is.
              - Standing defaults: (1) working slice first (save + send), polish later;
                (2) settings live in W-Soft › Settings (LittleFS JSON via WSoftSettingsService);
                (3) journey: idle → steady → saved (screen + web show "Saved #n"), and the
                blocked states (moving, alarm, empty, queue full, no Hub); (4) storage: NVS /
                LittleFS queue, no database on the ESP.
              - Cloud cannot run `pio run`; firmware builds and flashes happen on the bench
                laptop. Host tests (g++ + Unity) and the interface npm build run in the cloud.
SUCCESS:      1. (OWNER-CONFIRMED) W-Soft › Settings has an Auto-save switch and a "stable for
                 N seconds" field (DERIVED range 1–30, default 3), persisted across reboot.
              2. (OWNER-CONFIRMED) With auto-save on, a load steady for N s saves exactly once;
                 it saves again only after the scale goes back to empty.
              3. (OWNER-CONFIRMED) The ESP screen shows a SAVE button that saves on touch; it
                 is visibly disabled while moving / alarm / empty / no DJB.
              4. (DERIVED) The web Live page has a Save button and shows the last saved weighings.
              5. (DERIVED) Saved weighings are queued in flash and delivered to the Hub via
                 signed POST /api/v1/ingest/weighings; a resend is a Hub duplicate (200), not
                 a second row. Queue full = save refused with a visible message, never a drop.
              6. (DERIVED) Host unit tests cover the save rules (timer, once-per-load, blocked
                 states, manual save) and pass; `npm run build` of interface passes.
              7. (DERIVED) Bench: Jurien saves one weighing by touch and one by auto-save, and
                 both appear on the test Hub.
ASSUMPTIONS:  | assumption | conf | blast radius if wrong |
              | Board config exposes GT911 touch via board->getTouch() | M | medium: touch button deferred, web + auto-save still ship |
              | Hub accepts decimal kg (PR #78, confirmed by Jurien + coordinator 12:10) | H | low |
              | Device clock is valid (set from Hub Date header) for committed_at | H | medium: saves wait for clock |
              | Flash queue of 32 entries is enough between Hub contacts | H | low |
MISSION:      weighsoft-op-feature
DEFERRED:     - none for Hub units (resolved by Hub PR #78).
              - Wi-Fi OTA reliability check (fix 9d2199c pushed; verify on next Wi-Fi flash).
GATES:        - Kg mapping: resolved 12:10 (Jurien: Hub already takes 5.5 decimal kg)
              - Push to branch claude/project-thread-21cbr9 (draft PR #29): TOLD
              - Flash to bench ESP: TOLD (Jurien flashes on the laptop)
              DEMO: required — touch SAVE and auto-save are watchable on the bench and the Hub.
BUDGET:       standard
```

## Ledger

Critical path: builder (core rules + tests → ESP queue/send → settings → touch → web) → cloud
checks (host tests, interface build) → laptop build → bench demo. Start now: builder.

| # | Agent | Ticket | Status |
|---|---|---|---|
| 1 | builder (general-purpose) | full slice on claude/project-thread-21cbr9 | DONE_WITH_CONCERNS (b556145..80e52ef; ESP code not compiled in cloud) |
| 2 | laptop build (Remote Control) | pio run -e wsoft_waveshare43_ota, then flash | DONE (253bff7 built SUCCESS, firmware.bin 2,344,528 B, USB-flashed COM10 14:1x, hash verified, /rest/wsoftSave answers) |
| 3 | bench demo (Jurien) | SUCCESS 1-7 on the bench + Hub | DONE (Jurien 31 Oct 19:04 UTC: "the auto saves work"; bench test mode on because of the DJB EEPROM alarm) |

Notes: a parallel manual build at 12:58 clashed in the same .pio folder and forced a full rebuild
(~45 min); a Remote Control restart cut the next run off near the end. Rule for next time: one build
per folder, announced before it starts.

SITREP 1 (12:25): host tests 6/6 suites green (re-run by command post: all OK); interface build green;
touch GT911 from board config (SDA 8/SCL 9/INT 4); weighing_id wsoft-<mac>-<series>-<seq>;
exact decimal kg; manual SAVE needs 1 s steady, auto-save N s (default off, 3 s).
Critical path now: laptop compile → flash → bench demo (SUCCESS 7).
