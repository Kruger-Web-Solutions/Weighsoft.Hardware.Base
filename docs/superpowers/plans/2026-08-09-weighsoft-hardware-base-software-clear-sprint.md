# Sprint: Software clear → flash → field test list

**Created:** 2026-08-09  
**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Skill:** RhynoSprintPlanCreate  
**Architecture plan:** `docs/superpowers/plans/2026-08-09-esp8266-live-weight-architecture-options.md`  
**Prior sprints:**  
- `SPRINT-2026-08-09-HWB-A` — Option A lean field board (closed)  
- `SPRINT-2026-08-09-HWB-B` — field fix catalog + printer (code shipped; field checks open)  
- `SPRINT-2026-08-09-HWB-C` — WiFi weight discovery (P1–P4 shipped; P5 needs:human)  
**Sprint id:** `SPRINT-2026-08-09-HWB-D`  
**KPI log:** `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`  
**Questions store:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-questions.md`  
**Training log:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-training-log.md`  
**Status:** draft — **ready for flash + test** after Jurien approves (no remaining software build RTs)  
**List mode:** LOCAL LIST MODE (`$HOME/.cursor/skills/rhynoTodoList/`)

## Goal

Clear remaining **software** work for Hardware.Base; then **flash once**; then Jurien runs the **test list**.

## Locked decisions (carry-in)

- Option A lean field board; caps **9 products / 40 transactions**.  
- Buzzer / DI3 / DI4 dropped.  
- Discovery **Option 2**; **sender = any LAN device**.  
- Count is **job-wide** (not per PLU) until **RT-040** (later).  
- Printer config on **Target & Relays**, not Tech.  
- **Flash only after** software items for this repo are cleared.  
- Do **not** mix `RS485-CanHatPi5DJB-W1X` into this sprint.  
- Never commit `data/config/wifiSettings.json`.

## Data audit

| Check | Expected | Found | Result |
|-------|----------|-------|--------|
| Repo | `Weighsoft.Hardware.Base` | Workspace folder matches; one-repo sprint | PASS |
| Branch | `RelayBoardEspBuildIn` | `git branch --show-current` = `RelayBoardEspBuildIn` | PASS |
| Product option | Option A + Option 2 discovery | Architecture + Sprint C docs agree | PASS |
| Caps | 9 products / 40 tx | Unchanged | PASS |
| Dropped features | No buzzer; no DI3/DI4 | Still stripped / decisions complete | PASS |
| Waiting honest | Field tests still need Jurien | RT-005/007/008 (+ new TEST items) stay `waiting` | PASS |
| Wrong-repo items | No RS485 in this sprint | RS485 RTs left alone | PASS |
| Dirty tree known | Secrets + optional script | See below | PASS (noted) |
| List access | LOCAL LIST MODE | PC `list.yaml` present | PASS |
| Software backlog | No open non-later software RTs | Inventory Phase 1: **cleared** | PASS |

### Software inventory (this repo only)

| RT | Stage | Software? | Sprint D action |
|----|-------|-----------|-----------------|
| RT-005 / TEST Live Weight UI | waiting | Field | Stay waiting — test list |
| RT-007 / TEST DI actions | waiting | Field | Stay waiting — test list |
| RT-008 / TEST printer ticket | waiting | Field | Stay waiting — test list |
| RT-033 / RT-034 catalog + printer UX | complete | Done | — |
| RT-039 discovery | complete | Done (P5 field) | Field via RT-044 |
| RT-010 MQTT heap | todo / later | Later | Out of scope |
| RT-019 WiFi report export | todo / later | Later | Out of scope |
| RT-040 per-PLU count | todo / later | Later | Out of scope |
| Planned/build software RTs | — | **None** | Software cleared |

**Audit verdict: PASS — no remaining software build work.** Status = **ready for flash + test**. Gate asks Jurien to approve **flash then test** (not a feature-build sprint).

### Dirty tree audit (at plan write)

| Path | Note | Commit? |
|------|------|---------|
| `data/config/` (e.g. `wifiSettings.json`) | WiFi secrets from board FS | **Never commit** |
| `scripts/verify-rhyno-skills-clone.sh` | Unrelated untracked | Out of this sprint commit |
| Sprint D docs / KPI / questions / training log | This session | **Commit** |

## Phases

### Phase 1 — Inventory remaining software (bookkeeping)
- **RT:** list audit only (no new software RT)  
- **Scope:** Confirm every non-later HWB item is complete or waiting field. Document in this plan.  
- **Acceptance:** Table above accurate; no planned/build software left.  
- **Result:** **PASS — software cleared.**  
- **Depends on:** —

### Phase 2 — Small doc gaps only (no feature build)
- **RT:** docs only (firewall note, count expectation)  
- **Scope:** Real gaps only — Windows UDP **4210** firewall note in discovery doc; one line that count is **job-wide until RT-040**. No large features.  
- **Acceptance:** Docs match locked decisions; no firmware/UI feature work.  
- **Depends on:** Phase 1  

### Phase 3 — Prep flash checklist (no flash until approve)
- **RT:** RT-046 (planned)  
- **Scope:** Checklist for one flash after software clear: kill Python COM holders, flash/OTA, confirm board IP, confirm discovery REST + Tech find-me text, leave WiFi secrets out of git.  
- **Acceptance:** Checklist written; **flash not executed** until Jurien says approve flash.  
- **Depends on:** Phase 1–2  

### Phase 4 — Hand off HUMAN test list
- **RT:** RT-005, RT-007, RT-008, RT-043, RT-044, RT-045 (all `waiting`, `test_list: true`)  
- **Scope:** One TTS-friendly **### tests** block on the living list. Jurien runs after flash.  
- **Acceptance:** LIST.md shows tests clearly; agents do not mark complete without her word.  
- **Depends on:** Phase 3 flash done (her timing)  

### Phase 5 — Capture questions + training durability
- **RT:** RT-047… (questions); docs questions + training log  
- **Scope:** Durable Q store (list + repo md); training log for decisions + field outcome placeholders; KPI sprint D row.  
- **Acceptance:** Open/answered questions not chat-only; KPI updated.  
- **Depends on:** —  

## Out of scope

- **RT-040** per-PLU count (unless she pulls it)  
- **RT-010** / **RT-019** later items  
- Other repos (`RS485-CanHatPi5DJB-W1X`, etc.)  
- Option B large catalog  
- Feature build / flash in the plan-write session without her approve  

## Flash checklist (Phase 3 — do not run until approved)

1. Confirm software inventory still empty of planned/build RTs.  
2. `Get-Process python* | Stop-Process -Force` (free COM if USB flash).  
3. Flash preferred path (USB or HTTP/OTA to current board IP — historically `192.168.2.67`).  
4. Confirm login + Live Weight loads.  
5. Confirm Tech → **How senders find me** shows IP / UDP 4210 / mDNS.  
6. Confirm `GET /rest/liveWeightDiscovery` (authenticated) returns identity.  
7. Do **not** commit `data/config/wifiSettings.json`.  
8. Tell Jurien: board ready — start **### tests**.

## Test list (HUMAN — after flash)

| RT | Title | Notes |
|----|-------|-------|
| RT-043 | TEST: Product catalog LIVE persist | Leave Product tab / refresh; must stay LIVE board products (Sprint B RT-033) |
| RT-008 | TEST: Printer IP/port on Target & Relays + ticket | Find fields under 30s; ESC/POS ticket (Sprint B RT-034 / field RT-008) |
| RT-007 | TEST: DI Print / Next / Start / Stop | Map DI1/DI2; pin to GND; count++ / print events |
| RT-005 | TEST: Live Weight UI overall | Dial, tabs, zones, login |
| RT-044 | TEST: Discovery — How senders find me | Tech text + optional `scripts/listen-weighsoft-announce.py`; watch Windows firewall UDP 4210 |
| RT-045 | TEST: Count is job-wide | Expect shared job count until RT-040; not per-PLU |

## Improvements (KPI — required)

| KPI id | Improvement | Source RT / finding | Status | Sprint |
|--------|-------------|---------------------|--------|--------|
| KPI-017 | Durable test list + questions section on living list (TTS) | Jurien ask 2026-08-09 | open | HWB-D |
| KPI-018 | Training log for decisions + field outcomes (KPI/training data) | Jurien ask 2026-08-09 | open | HWB-D |
| KPI-019 | Windows firewall note for UDP 4210 listen harness | Discovery field risk | open | HWB-D |

## Pre-build / pre-flash gate

- [x] Data audit all PASS  
- [x] Plan matches decisions (Option A / Option 2 / caps / later RT-040)  
- [x] rhynoTodoList: software cleared; flash RT-046 **planned**; tests **waiting**; questions stored  
- [x] Improvements section seeded (KPI-017…019)  
- [ ] Jurien approved sprint D / said **approve flash** / **start tests**  
- [x] No other-repo work  

**Gate status:** bookkeeping complete. **Not open for flash** until Jurien approves.

## Jurien decisions needed

1. Approve **SPRINT-2026-08-09-HWB-D** (software clear → flash → test)?  
2. Approve **flash** now (or say when)?  
3. After flash — start the **test list** (RT-005/007/008/043/044/045)?  
4. Optional open Qs: Windows firewall UDP 4210 OK? Which **first real sender** product next?
