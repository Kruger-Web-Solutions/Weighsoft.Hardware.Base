# Plan: switches everywhere + RS485 scale setup (one build, parallel agents)

Design (approved by Rhyno, 7 Nov 2026 21:06 UTC): `docs/superpowers/specs/2026-11-07-wsoft-switches-scale-setup-design.md`.
Rhyno: "build it in 1 big step", "start multiple agents and get it done".

Three agents work in parallel against the **contracts below**. A contract is fixed: if an agent finds one is
wrong, it stops and reports, it does not invent a variant.

| Agent | Repo / folder | Owns |
|---|---|---|
| **FW** | Hardware.Base `lib/`, `src/`, `test/`, `docs/`, `platformio.ini` | board logic, REST, screen, Hub command client |
| **UI** | Hardware.Base `interface/` only (own worktree) | browser pages |
| **HUB** | Weighsoft.Indicator.Hub | command table, routes, signed device pull, Hub web |

Branch everywhere: `claude/project-thread-d3fchs`. Hardware.Base PR #32 (base `claude/project-thread-9f4l7x`) grows.
Hub: a new draft PR from that branch into `main`.

---

## Contract A: board REST (FW serves it, UI calls it)

All JSON. "admin" = `AuthenticationPredicates::IS_ADMIN`, "user" = `IS_AUTHENTICATED`. Errors are
`409` (or `400` for a bad shape) with `{"error":"CODE","message":"plain words"}`.

### A1. `GET /rest/wsoftSwitches` (user)
```json
{
  "source": "rs232",
  "approval": {"on": false, "audit_counter": 3, "trade_check": "approval_off",
               "can_switch_on": true, "blocked": null},
  "auto_save": false,
  "pin_set": true,
  "pin_locked_s": 0,
  "log": ["4 2026-11-07T21:00:00Z OFF->ON by=rhyno where=browser why=bench test"]
}
```
`blocked` is `null` or a code (`SETUP_INCOMPLETE`). `log` is the last 8 approval log lines, oldest first.

### A2. `POST /rest/wsoftSwitches` (admin)
Body: `{"switch":"approval"|"auto_save","on":true|false,"reason":"text"}`.
- `reason` is required for approval (1 to 80 printable characters, trimmed), optional for auto_save.
- `who` = the signed-in username (from `securityManager->authenticateRequest(request).user->username`).
- `where` = `browser`.
- `200` = the A1 body after the change. `409` codes: `ALREADY`, `SETUP_INCOMPLETE`, `REASON_REQUIRED`, `REASON_TOO_LONG`, `BUSY`.

### A3. `POST /rest/wsoftPin` (admin)
Body: `{"pin":"1234"}` (4 to 6 digits). `200` `{"pin_set":true}`. `400` `PIN_FORMAT`.
Stored as a salted SHA-256 (random 16-byte salt), NVS. Never returned, never logged.

### A4. `GET /rest/wsoftScaleSetup` (user)
```json
{
  "source": "rs485",
  "editable": true,
  "locked": null,
  "complete": false,
  "missing": ["sa_number", "max_kg", "blank_below_e"],
  "setup_audit": 2,
  "board": {
    "sa_number": "", "max_kg": "", "min_kg": "",
    "blank_below_e": null, "blank_above_e": 9,
    "stable_samples": 3,
    "tare_enabled": true, "tare_max_kg": "", "preset_tare_kg": ""
  },
  "card": {"division": 1, "decimal": 0, "unit": 1}
}
```
- `locked` is `null`, `"APPROVAL_ON"` or `"RS232"`. With `source` `rs232` the page shows "Set on the indicator" and nothing else.
- `card` is the last values read from the weighing card (`null` when not read yet). Card settings are read and written through the
  **existing** `POST /rest/djb` (no second path). The UI shows division, decimal, unit, zero_track_limit,
  zero_track_time, stability_range and stability_time with read and write buttons through `/rest/djb`.
- kg values are decimal text, `""` = not set. `blank_below_e` and `blank_above_e` are counts of e.

### A5. `POST /rest/wsoftScaleSetup` (admin)
Body: `{"board":{...same keys as A4.board...},"reason":"text"}`. `200` = the A4 body. Codes:
`SETUP_LOCKED` (approval ON or RS232), `SETUP_ORDER` (needs `max > e`, `min >= e`, `min < max`), `SETUP_NUMBER`
(not a number or not a multiple of e or more than 5 decimals), `SETUP_TEXT` (SA number: 1 to 32 printable characters),
`SETUP_RANGE` (stable_samples 3..20, blank counts 0..100, tare_max ≤ max, preset ≤ tare_max), `REASON_REQUIRED`.
Every saved change and every successful card write through `/rest/djb` adds 1 to `setup_audit`.

`complete` = sa_number, max_kg, min_kg and blank_below_e are set and valid, and the card's division and decimal have been read.
(blank_above_e defaults to 9 and stable_samples to 3.)

### A6. Existing endpoints
- `/rest/wsoftSave` stays, but loses `auto_save` (now in A1).
- `/rest/wsoftSettings` loses `auto_save` (field, JSON, settings test, Settings checkbox removed).
- `/rest/djb` writes also refused when RS232 (no card) and count into `setup_audit`.

---

## Contract B: board ↔ Hub commands (FW and HUB)

### B1. Device pull: `POST /api/v1/ingest/commands`
Signed exactly like every ingest request (prefix `scale-models-hub-v1`, four `X-ScaleModels-*` headers), behind the
same ingest chain (rate limit, ingest log, raw body, HMAC + nonce).

Request body (exact keys, zod `.strict()`):
```json
{"applied_through": 12, "results": [{"id": 12, "ok": false, "error": "SETUP_INCOMPLETE"}]}
```
- `applied_through`: integer 0..9007199254740991, the highest command id the board has handled.
- `results`: 0 to 20 items. `error` is `null` when `ok`, else a code `[A-Z_]{1,32}`.

Hub, in one transaction:
1. For each result whose command belongs to this device and is still `pending`, set the status to `done` or `refused`,
   with `error` and `resolved_at`.
2. Mark this device's `pending` commands older than 600 s as `expired`.
3. Answer `200` with the pending commands of this device with `id > applied_through`, oldest first, max 10:
```json
{"commands":[{"id":13,"switch":"approval","on":true,"by":"admin","reason":"bench test"}]}
```
`switch` is `approval` or `auto_save`. `reason` is `""` when none.

The response is signed like setup pages (`server/src/setup/responseSigning.ts`), with its **own prefix**
`scale-models-hub-command-v1`. The four headers carry the request's key id and nonce, a fresh timestamp, and HMAC key
`sha256(pairing secret)`. `Content-Type: application/json` exactly.
`404 UNKNOWN_DEVICE`, `400 INVALID_COMMANDS_REQUEST`.

### B2. Board side
- Every 10 s while online and not in OTA, the Hub task posts B1.
- It checks the response:
  - key id equal to its own;
  - nonce equal to the one it sent;
  - signature = `sha256=` + hex HMAC-SHA256(key, prefix\nkey_id\ntimestamp\nnonce\nbody);
  - status 200.
  On any mismatch it drops the response and logs `HUB commands bad signature`.
- It applies each command in id order through the same switch function as the screen and browser (`where=hub`,
  `who=by`, `reason`). It keeps each result for the next pull.
- `applied_through` lives in NVS (`hub` namespace, key `cmd`) and is saved after each apply.
- After any switch change, from anywhere, the Hub task sends a heartbeat at once.
- The heartbeat `health` gains `"auto_save":true|false` and `"scale_setup_complete":true|false`
  (approval, trade_check and audit_counter are already there).

### B3. Hub admin routes (HUB only, behind `requireSession`, mutating ones need `X-Requested-With: hub`)
- `POST /api/v1/devices/:deviceId/commands`
  - body `{"switch":"approval"|"auto_save","on":bool,"reason":string}`;
  - approval needs a reason of 1 to 80 characters;
  - `by` = the session username.
  - `201` = the command row.
  - A new command for the same device and switch marks any older `pending` one for that switch `refused` with
    `SUPERSEDED`, so the last click wins.
- `GET /api/v1/devices/:deviceId/commands?limit=20`: newest first, `{"items":[{id, switch, on, by, reason, status, error, created_at, resolved_at}]}`.
- The device detail read model gains `switches`, from the latest heartbeat health: `{"approval": "on"|"off"|null,
  "auto_save": bool|null, "audit_counter": number|null, "scale_setup_complete": bool|null}`.

---

## FW tasks (TDD: each pure rule gets a failing Unity test first)

1. **wsoft_core `wsoft_switch.{h,cpp}`** (pure):
   - request check: which, on, who, where, reason;
   - error codes as in A2;
   - approval ON gate: `rs485 && !setup_complete` gives `SETUP_INCOMPLETE`;
   - log line format as in A1.
   - Test: `test/test_wsoft_switch`.
2. **wsoft_core `wsoft_pin.{h,cpp}`**:
   - PIN format;
   - salted hash: SHA-256 from mbedtls on the ESP; for the host tests a small SHA-256 in `wsoft_core` used by both,
     checked against NIST vectors;
   - wrong-try counter: 5 tries, then a 300 s lock.
   - Test: `test/test_wsoft_pin`.
3. **wsoft_core `wsoft_scale_setup.{h,cpp}`**:
   - parse and check A5 (kg text vs e from division and decimal);
   - `complete` and `missing`;
   - blanking test: `blank(value) = value < -blank_below_e*e || value > max + blank_above_e*e`;
   - below Min;
   - tare math: tare only when stable, ≤ tare_max and enabled; net = gross − tare;
   - a stable-samples counter.
   - Test: `test/test_wsoft_scale_setup`.
4. **wsoft_core `wsoft_hub_commands.{h,cpp}`**:
   - build the B1 request JSON;
   - parse the B1 response (commands array, strict);
   - build the signing input (the HMAC itself can use the same small SHA-256, HMAC per RFC 2104, with RFC 4231 vectors).
   - Test: `test/test_wsoft_hub_commands`.
5. **lib/wsoft_esp `switches.{h,cpp}`**:
   - one mailbox, applied by the **screen task** (the only flash writer):
     `switches_set(which, on, who, where, reason, timeout_ms)` waits for the answer, like `weighings_save`;
   - `switches_info()`.
   - Approval goes through a new Base-only `approval_set(...)` in approval.cpp (inside `#ifdef WSOFT_ON_BASE`, so
     `unifdef -UWSOFT_ON_BASE` still matches the original). It reuses `write_change`.
   - The USB `APPROVAL ON|OFF` line also goes through `switches_set(where=usb)`.
   - Auto save moves to NVS (`weigh` namespace, key `auto`) and is applied in `weighings.cpp`.
   - Update the copy check in `docs/WSOFT-WAVESHARE43.md` for approval.cpp (now unifdef like hub.cpp).
6. **Scale setup on the board**:
   - NVS `scale` namespace;
   - the screen task reads the card's division and decimal at boot and after a card write;
   - blanking shows `----`, is not for trade and refuses a save;
   - below Min refuses a save while approval is ON;
   - stable samples are added to the existing stability;
   - tare state (runtime): weighings send gross, tare, net and `basis` `NT` when tared (extend `wsoft_weighing_json`
     and its test).
7. **REST** A1 to A5 in `WSoftStatusService` (or one new `WSoftSwitchesService`), plus the A6 changes. Remove
   `auto_save` from `WSoftSettings` and its test.
8. **Screen**:
   - a MENU button (CYD: the left bottom box; Waveshare: the same place in its layout) opens a switches page with
     Approval, Auto save, TARE (RS485 only) and Back;
   - Approval opens a number pad (0 to 9, delete, OK, cancel) and checks the PIN through `wsoft_pin`;
   - the plate line shows SA no., Max, Min and e on RS485 when set.
9. **Hub client** B2 in hub.cpp (inside `WSOFT_ON_BASE` guards), plus the heartbeat fields.
10. Docs: `docs/WSOFT-CYD28.md` and `docs/WSOFT-WAVESHARE43.md`: the new endpoints and rules, and drop
    "Approval has no web write path".

**Checks before done:**
- `pio test -e native`: every suite passes, and each new suite has been seen to fail once.
- `unifdef -UWSOFT_ON_BASE` of approval.cpp and hub.cpp still equals the original (script below).
- No ESP toolchain in the cloud: the laptop builds `wsoft_cyd28` and `wsoft_waveshare43`.

## UI tasks (interface/ only)

- `types.ts` and `api.ts`: A1 to A5.
- **Live page**: replace the "Approval OFF (switched with a USB cable only)" row and the Save panel's auto-save
  text with a `WSoftSwitches` card:
  - two switches;
  - an approval dialog asking the reason;
  - the blocked reason in plain words;
  - the log (collapsed);
  - poll every 2 s;
  - non-admins see the state read-only.
- **Settings tab**:
  - remove the Auto save checkbox;
  - add "Approval PIN" (admin): set or change, 4 to 6 digits, shows "PIN set" or "No PIN yet".
- **New "Scale setup" tab** (admin to edit, user to read):
  - RS232: one line, "Set on the indicator".
  - RS485:
    - the board fields form (A5) with a reason;
    - a card section using `/rest/djb` read/write per field;
    - complete/missing chips;
    - when locked, everything is read-only with "Switch Approval off to change the setup".
- `npm run lint` and `npx tsc --noEmit` clean. Plain words. No "legal".

## HUB tasks

Follow the Hub `CLAUDE.md` (intel packet, gate script, real-SQLite tests, problem+json, OpenAPI + `IMPLEMENTED_ROUTES`).

1. ADR 0007 "Board switches from the Hub: device-pulled signed commands".
2. Schema: a `device_command` table (idempotent `CREATE TABLE IF NOT EXISTS`, FK to device, CHECKs on switch and status, index `(device_id, status, command_id)`).
3. B1 route + B3 routes + the device detail `switches` field, with tests (signature fixture: sign a known body under the command prefix and pin the bytes).
4. Web: a "Switches" card on the device page:
   - Approval and Auto save state;
   - buttons with a confirm dialog (a reason for approval);
   - "Waiting for the board…" while pending;
   - the last 20 commands with their status.
5. `cd server && npm test && npm run typecheck && npm run build`; `cd web && npm test && npm run build && npm run lint`.
