# Sprint: Option A — Lean field board

**Created:** 2026-08-09  
**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Skill:** RhynoSprintPlanCreate  
**Architecture plan:** `docs/superpowers/plans/2026-08-09-esp8266-live-weight-architecture-options.md`  
**Sprint id:** `SPRINT-2026-08-09-HWB-A`  
**KPI log:** `~/.cursor/skills/RhynoSprintPlanCreate/kpi.yaml`  
**Status:** in_build  
**Last RhynoSprintPlanCreate run:** 2026-08-09 15:51  
**Execute opened:** 2026-08-09 via `/weighsoft-plan-execute`

## Goal

Ship a stable Live Weight + relay field board: no LED demo clutter, hard serial path, live twin, clear printer UX, then **9 products** and **40 transactions** — **no buzzer**.

## Data audit (re-run 2026-08-09 15:51)

| Check | Expected | Found | Result |
|-------|----------|-------|--------|
| Repo | Weighsoft.Hardware.Base | Workspace path matches | PASS |
| Branch | RelayBoardEspBuildIn | `git branch` = RelayBoardEspBuildIn | PASS |
| Product option | Option A | Architecture plan Decision = Option A | PASS |
| Caps | 9 products / 40 tx | RT-018 + architecture plan | PASS |
| Buzzer | Stripped | No `buzzer`/`tone` in `src/`; strip still **uncommitted** | PASS |
| DI count | 2 only | RT-013 complete; DI1/DI2 only | PASS |
| LED Example | Still present until RT-012 | Still in `main.cpp` + LedExampleService | PASS (work correct) |
| OTA IP | Should become 192.168.2.67 | Still `192.168.3.117` in esp12e_ota — RT-009 | PASS (work correct) |
| Waiting honest | Field checks need Jurien | RT-005/007/008 waiting | PASS |
| Wrong-repo items | None in this sprint | RS485 RT-022/025/026 excluded | PASS |
| Planned set | Sprint RTs planned | RT-012/020/014/015/016/017/009/021/018 planned | PASS |
| Dirty tree | Known | Buzzer strip + docs + this sprint file uncommitted; commit in sprint hygiene | PASS (noted) |

**Audit verdict: PASS** — plan ready. **Gate blocked only on Jurien approve / build.**

## Phases (build order)

### Phase 1 — Strip LED Example
- **RT:** RT-012  
- **Scope:** Remove LED Example from firmware (`LedExampleService`, `main.cpp`) and UI (menu/routes).  
- **Acceptance:** No `/project/led-example` route; build succeeds for `esp12e`; heap/flash no LED symbols.  
- **KPI:** KPI-004  
- **Depends on:** —

### Phase 2 — Harden scale serial
- **RT:** RT-020  
- **Scope:** Confirm/enforce rate-limit, UNCHANGED on same weight, safe parse under continuous UART.  
- **Acceptance:** Documented behaviour; no WDT under sustained scale lines in bench note or code comments + stable publish ≤5 Hz.  
- **KPI:** KPI-005  
- **Depends on:** Phase 1 preferred (flash/RAM headroom)

### Phase 3 — Twin live + stats
- **RT:** RT-014, RT-015  
- **Scope:** Twin WS drives real relays/DI when online; ESP stats/bottom blocks show real data.  
- **Acceptance:** Online board: click RY toggles hardware; DI chips follow pins; heap/VCC/WiFi/uptime not stuck on demo when connected.  
- **KPI:** KPI-002  
- **Depends on:** Phase 1–2 helpful not hard

### Phase 4 — Tech Connect/Stop stream
- **RT:** RT-016  
- **Scope:** Connect toggles to Stop; stream box shows incoming lines while connected.  
- **Acceptance:** Admin Tech tab: Connect → lines appear; Stop → stream halts.  
- **KPI:** KPI-003  
- **Depends on:** Phase 2

### Phase 5 — Printer UX
- **RT:** RT-017  
- **Scope:** Network printer IP/port obvious on Target & Relays (not cut off / easy to miss).  
- **Acceptance:** Jurien can find IP/port without scrolling hunt; labels clear.  
- **KPI:** KPI-001  
- **Depends on:** —

### Phase 6 — OTA IP + docs
- **RT:** RT-009, RT-021  
- **Scope:** `esp12e_ota` `upload_port` → `192.168.2.67`; docs: no temp/power meter; no buzzer.  
- **Acceptance:** OTA ini matches board IP; RELAY-BOARD / twin docs match product (no buzzer wire guide as a feature).  
- **Depends on:** —

### Phase 7 — Catalog + transaction log
- **RT:** RT-018  
- **Scope:** Max **9** products; max **40** transactions (ring/NDJSON drop oldest); Save rewrite only.  
- **Acceptance:** Cannot exceed caps; oldest tx dropped at 41; products Save persists; heap stable.  
- **Depends on:** Phase 1–2

### Phase 8 — Field verify (Jurien / desk)
- **RT:** RT-005, RT-007, RT-008 (stay **waiting** until she tests)  
- **Scope:** UI look, DI actions, printer ticket.  
- **Acceptance:** She confirms each; then move to complete.  
- **Depends on:** Phases 1–7 flashed on board

## Out of scope (this sprint)

- **RT-010** MQTT off — later  
- **RT-019** WiFi report export — later (after catalog)  
- Buzzer / GPIO15 feature — dropped forever for this product line unless new RT  
- DI3/DI4 — dropped (RT-013)  
- Any **RS485-CanHatPi5DJB-W1X** / RhynoV4 work  

## Improvements (KPI — required)

Seeded in `kpi.yaml`. Agents **must** add new `KPI-###` rows when they find more during build.

| KPI id | Improvement | Source | Status |
|--------|-------------|--------|--------|
| KPI-001 | Clearer Network printer block | RT-017 | open |
| KPI-002 | Twin leaves Demo when WS online | RT-014 | open |
| KPI-003 | Tech Connect/Stop + stream | RT-016 | open |
| KPI-004 | Remove LED Example (flash/RAM) | RT-012 | open |
| KPI-005 | Serial rate-limit / change-gate | RT-020 | open |

## Pre-build gate

```
GATE:
- [x] Data audit all PASS (or FAIL fixed + re-audited)
- [x] Plan doc matches Option / caps / dropped features
- [x] Sprint items are planned in rhynoTodoList for this repo only
- [x] Improvements section exists (at least seed KPIs)
- [x] Jurien approved sprint (or explicit "build" / "go") — `/weighsoft-plan-execute`
- [x] No work from other repos in this sprint
```

**Gate status: OPEN** — executing Phase 1.

## Jurien decisions needed

1. Printer IP for RT-008 when ready (Phase 8)  
2. Field checks RT-005 / RT-007 after flash  


