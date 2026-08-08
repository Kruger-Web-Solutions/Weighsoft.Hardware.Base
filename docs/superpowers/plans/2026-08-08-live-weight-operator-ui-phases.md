# Plan: Live Weight operator UI + DI + network print

**Spec:** `docs/superpowers/specs/2026-08-08-live-weight-operator-ui-di-actions-handoff.md`  
**Integration base:** `RelayBoardEspBuildIn` (not `master`)  
**Gate policy:** `gate=human` after each phase PR lands on the integration branch (Jurien sign-off). Hardware flash = always HUMAN.  
**Auto-advance:** only after human OK, or after green software gate when user said continue.

## Phase DAG

```text
P1 Firmware contract ──► P2 Operator UI ──► P3 Twin + docs + cleanup ──► P4 Device flash/verify (HUMAN)
```

---

### P1 — Firmware contract (DI, print, job, stability)

**Branch:** `feat/lw-p1-firmware-di-print` from `RelayBoardEspBuildIn`  
**Depends:** none  
**Scope:**
- Persist/expose `di1_action`, `di2_action`, `job_running`, `last_action`, `action_seq`
- Persist `printer_enabled`, `printer_ip`, `printer_port` (default 9100)
- RelayBoard DI rising-edge → LiveWeight action handler
- Actions: `none` | `print` | `next` | `start` | `stop`
  - `next` → piece `count++`, refresh total
  - `print` → WS event + short TCP ESC/POS to printer IP:port (fail soft)
  - `start`/`stop` → `job_running`
- Confirm buzzer `tone`/`noTone` on GPIO15
- Rate-limit Live Weight WS/state churn; prefer simple weight parse when regex empty/fail
- Do not add BLE; do not claim RS-485 works

**Acceptance criteria:**
1. `esp12e` firmware builds (`platformio run -e esp12e`)
2. REST/WS JSON includes new fields; config persists across reboot (printer + DI actions)
3. Simulated/logic path: DI edge with action `next` increments count
4. Print with empty IP fails soft (status message, no crash)
5. clang-format clean on touched C++ files

---

### P2 — Operator UI (tabs + hero dial + navy)

**Branch:** `feat/lw-p2-operator-ui` from post-P1 base  
**Depends:** P1 DELIVERED  
**Scope:**
- Tabs: Live | Target & Relays | Product | Tech (admin)
- Live: hero dial only + Net + read-only PLU strip (no forms)
- Target & Relays: range, relay maps, DI actions, printer IP/port/enable, buzzer test
- Product: PLU / description / count / total
- Tech: today’s Setup (source/baud/regex); hide RS-485 as working
- Navy styling via `liveWeight.css`

**Acceptance criteria:**
1. `cd interface && npm run build` succeeds
2. Routes match tab IA; Live has no job setup forms
3. Types match firmware JSON keys
4. Dial CSS max size ≥ 480px (hero), navy tokens used on new tabs

---

### P3 — Twin labels + board docs + light cleanup

**Branch:** `feat/lw-p3-twin-docs` from post-P2 base  
**Depends:** P2 DELIVERED  
**Scope:**
- Twin shows DI1/DI2 configured action labels when Live Weight WS connected
- Update `docs/RELAY-BOARD-ESP-BUILT-IN.md` (tabs, DI actions, print, buzzer tone)
- Optional: strip Live Weight MQTT subscribe if still cheap win (do not break build)
- Mock server field parity

**Acceptance criteria:**
1. Docs match shipped UI/firmware
2. Twin builds; DI action tip/label visible in code path
3. `esp12e` still builds after cleanup

---

### P4 — Device flash & field verify (HUMAN)

**Depends:** P3 DELIVERED  
**Scope:** USB flash (kill python → COM + IO0/RST), verify dial, band relays, DI actions, buzzer sound, print to printer IP if available.

**Acceptance criteria (human):**
1. Board boots stable after flash
2. Buzzer audible when enabled
3. DI Print/Next behave as configured
4. Optional: ticket prints to network printer

---

## Status tracking

See `docs/plan-execution-status.md`
