# W-Soft: Approval and Auto save switches everywhere, plus RS485 scale setup (design)

Date: 2026-11-07. Status: **draft, waiting for Rhyno's approval**. Nothing is built.
Repos: Weighsoft.Hardware.Base (board firmware and browser UI) and Weighsoft.Indicator.Hub (Hub).

## Rhyno's decisions (7 Nov 2026, in the thread)

| # | Question | Answer |
|---|---|---|
| D1 | Which switch works from the screen, the Hub and the browser? | **Both** Approval and Auto save, all connected and in sync |
| D2 | How does the screen guard the Approval switch? | **PIN on the screen** (set in the browser by an admin). Auto save needs no PIN |
| D3 | Do the browser and Hub also ask for the PIN? | **No.** Admin login is enough, plus a short reason. Their name is logged |
| D4 | Where does the scale setup live? | **RS485 (our weighing card): on W-Soft.** RS232: everything (weight, tare, zero, calibration, SA no., setup) lives in the indicator |
| D5 | What does "tare" mean on RS485? | **Both:** a TARE button on the screen, plus a typed preset tare |

His standing rules for this job: read the code first, no double code or functions, integrate, remove what is not needed, no shortcuts.

## What exists today (read from the code)

- **Approval** (`lib/wsoft_esp/src/approval.*`): ON/OFF in NVS namespace `approval`, audit counter, last 8 log lines, trade queue. The only switch is a USB line: `APPROVAL ON|OFF <technician> <reason>`. The docs say "Approval has no web, MQTT or BLE write path. That is on purpose." **D1 to D3 change that rule.** Every switch stays logged and counted.
- **Auto save** (PR #32): `auto_save` is a field of `WSoftSettings` (framework JSON file). It is switched with a checkbox in the browser Settings form. The screen and the Hub can't switch it.
- **Hub**: knows nothing about approval or auto save. The heartbeat `health` object already carries `approval`, `trade_check` and `audit_counter`. The Hub stores it in `device_heartbeat` but doesn't show it. The Hub can only answer a device; it never reaches in (constitution 1, ADR 0003). Setup sync (ADR 0005) is the pattern: the device asks with a signed request, and the Hub answers with a **signed** response, because a LAN Hub can be plain http.
- **RS485 weighing card** (`lib/wsoft_core/src/wsoft_djb.*`, `POST /rest/djb`): the board can already read and write these card registers: unit, decimal, division, stability range/time, zero track limit/time, zero display, sampling, filters, calibration (`ad`). Writes are refused while Approval is ON. The card has **no** register for Max, Min, SA number or tare.
- **Old Indicator scale setup** (Weighsoft-Indicator `docs/superpowers/specs/2026-10-01-scale-setup-design.md`): it reuses its rules and checks: `max > e`, `min >= e`, `min < max`, stable samples 3 to 20, audit counter on every change, and "not a certificate".

## Design

### 1. One owner for each switch: the board

The board holds the true state of Approval and Auto save. Every change, from anywhere, goes through **one** board function:

```
switch_set(which = approval | auto_save, on, who, where = screen | browser | hub | usb, reason)
```

It checks the rules, stores the result, writes the log line and moves the audit counter (Approval only). The screen, the browser, the Hub and the USB line all call it, so the logic exists once.

- **Auto save moves out of `WSoftSettings`** into the same NVS store as Approval. The browser Settings checkbox is removed (no double switch). The `auto_save` field and its settings test go.
- The USB technician line stays as one more caller.

**Sync.** All three places show the board's real state, never their own copy:

| Place | Sees a change | How |
|---|---|---|
| Screen | at once | reads the board state |
| Browser | within 2 s | Live page polls `GET /rest/wsoftSwitches` |
| Hub | within about 10 s | the board sends a heartbeat right after any change. `health` gains `auto_save` |

**Last change wins.** A switch is an action, not a wish list, so two places can't fight.

### 2. Rules for Approval ON

- **RS485:** Approval can only go ON when the scale setup is complete and valid (section 4). The setup is **locked while Approval is ON**. To change it, switch Approval OFF first, which is logged and counted.
- **RS232:** Approval can go ON with no setup page, because the indicator owns the setup.
- Switching does what it does today: the engine resets, queued trade weights stay queued, the audit counter goes up by 1, and a log line is written.

### 3. The three places

**Screen**
- A small **MENU** button opens a switches page with Approval ON/OFF, Auto save ON/OFF and TARE (RS485 only).
- Approval opens a **number pad**. It wants the PIN, then switches. 5 wrong PINs lock the pad for 5 minutes, and each wrong try is logged.
- Auto save switches with one tap.
- The log says `where=screen`, `who=PIN`.

**Browser**
- The Live page gets an Approval switch and an Auto save switch. Both need an admin login.
- Approval asks for a short reason, and the log stores the admin's username.
- The **Approval PIN** is set or changed on the Settings page (admin, 4 to 6 digits). Only a salted hash is stored. The PIN is never shown, logged or sent anywhere.

**Hub**
- The device page shows Approval and Auto save from the latest heartbeat, plus a switch for each. A signed-in admin uses them, and Approval asks for a reason.
- A click stores a **command** row on the Hub: id, device, switch, on/off, admin, reason, time, status.
- The board asks the Hub for its commands every 10 s with a new signed request, `POST /api/v1/ingest/commands`, body `{"applied_through":N}`. The Hub answers with **signed** bytes (same signing as setup sync, its own prefix) listing commands newer than N.
- The board applies each command through `switch_set(..., where=hub, who=<admin>)` and reports the result on its next ask. The Hub marks each command done or refused, with the reason (for example "setup incomplete").
- A command not picked up within **10 minutes** expires, so a board that comes back online hours later never flips.

### 4. Scale setup page (browser, RS485 only, admin)

| Setting | Stored | Notes |
|---|---|---|
| SA approval number | board | text. Shown on the screen plate |
| Max, Min | board | in kg. `max > e`, `min >= e`, `min < max` |
| e (division), decimals, unit | **card** | written to the card with the existing DJB plan. No second copy |
| Zero tracking limit and time | **card** | as above |
| Stable range and time (card) + stable samples (board) | both | samples 3 to 20, default 3 |
| Blank below zero | board | default to be confirmed against the SA rule (see open items) |
| Blank above Max | board | default Max + 9e (the usual OIML R76 point, to be confirmed) |
| Tare on/off, most tare allowed, preset tare | board | D5 |
| Calibration | **card** | existing `ad` zero/load, locked while Approval is ON |

- **Locked while Approval is ON.** Every saved change moves the audit counter.
- The **screen plate** shows SA no., Max, Min and e, like an approved scale's plate.
- **The board applies:**
  - blanking: shows `----`, not for trade, no save;
  - below Min: not for trade, a save is refused while Approval is ON;
  - stable samples.
- **Tare:**
  - The TARE button works only when stable and within the most tare allowed. Tapping again clears it.
  - A preset tare can be picked instead.
  - Weighings send `gross_kg`, `tare_kg`, `net_kg` and `basis` NT when a tare is set. The Hub already accepts these.
- **RS232:** the page says "Set on the indicator" and shows nothing to edit.

### 5. What gets removed

- The `auto_save` field in `WSoftSettings`, its Settings checkbox, and the settings test for it. It moves to the switches store.
- The doc line "Approval has no web write path", replaced by the new rules.

## Errors (named, shown in plain words)

- `SETUP_INCOMPLETE`: Approval can't go ON before the RS485 setup is complete.
- `SETUP_LOCKED`: setup can't change while Approval is ON.
- `PIN_WRONG`, `PIN_LOCKED`, `PIN_NOT_SET`
- `ALREADY`: nothing changed.
- `COMMAND_EXPIRED`
- `TARE_NOT_STABLE`, `TARE_TOO_BIG`

## Testing

- **Host (Unity), test first:**
  - `switch_set` rules: who and where, setup gate, last change wins.
  - PIN hash, tries and lock.
  - Setup checks: order rules, card vs board split, blanking and Min.
  - Tare math.
  - Hub command parsing and the signature check.
- **Hub (vitest, real SQLite):**
  - command create, pull, sign, apply and expire;
  - the new route goes in OpenAPI and `IMPLEMENTED_ROUTES`;
  - the signed-response fixture.
- **Bench:** switch from each of the three places and watch the other two follow. Test the PIN lock, the RS485 gate, and tare on a real weighing.

## Open items (not blocking the design)

1. Default "blank below zero" point: needs the SA/NRCS rule. Until confirmed, it is a setting with no guessed legal claim.
2. The CYD board has no RS485 hardware. RS485 setup is tested on the Waveshare 4.3 board with the weighing card.
3. Delivery order: (a) board switches + browser, (b) screen menu + PIN, (c) Hub commands, (d) RS485 setup + tare. Each one is a PR.
