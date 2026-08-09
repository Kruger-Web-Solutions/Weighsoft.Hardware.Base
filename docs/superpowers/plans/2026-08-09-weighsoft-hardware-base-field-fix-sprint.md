# Sprint: Field fix — catalog persist + printer findable

**Created:** 2026-08-09  
**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Skill:** RhynoSprintPlanCreate  
**Architecture plan:** `docs/superpowers/plans/2026-08-09-esp8266-live-weight-architecture-options.md`  
**Prior sprint:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-option-a-sprint.md` (P1–P7 delivered; P8 field verify open)  
**Sprint id:** `SPRINT-2026-08-09-HWB-B`  
**KPI log:** `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`  
**Status:** in_build (Jurien approved; P1–P2 shipping on `sprint-b/field-catalog-printer`)  
**List mode:** LOCAL LIST MODE (`$HOME/.cursor/skills/rhynoTodoList/`)

## Goal

Make **Product catalog save stick** on the real board, make **printer IP/port** obvious (Target & Relays), then finish field checks RT-005 / RT-007 / RT-008.

## Why this sprint (field evidence 2026-08-09)

1. **Catalog UI lies in demo:** Board REST already has `S1 Sand`, `R2 Rock`, `W3 Water` (`GET /rest/liveWeightProducts` → count 3). Product tab screenshot still showed demo list `19 Screw M6` / `21 Washer` (hardcoded `DEMO_CATALOG`). Leave tab → remount → demo again. Firmware persist works; **UI demo path does not**.
2. **Printer IP/port:** Code is on **Target & Relays** (`#network-printer`). Tech tab has no printer fields — Jurien looked there / still cannot find them. RT-017 “done” in code, **not done for the operator**.
3. **Prior Option A sprint:** P1–P7 delivered (PR #6–#10). Flash done USB COM4. Still **waiting:** RT-005 UI, RT-007 DI, RT-008 printer ticket.

## Data audit

| Check | Expected | Found | Result |
|-------|----------|-------|--------|
| Repo | Weighsoft.Hardware.Base | Workspace folder matches | PASS |
| Branch | RelayBoardEspBuildIn | `git branch` = RelayBoardEspBuildIn | PASS |
| Product option | Option A, no buzzer, 2 DI | Still Option A in architecture plan; buzzer stripped | PASS |
| Caps | 9 products / 40 tx | Firmware + API enforce; board has 3 products saved | PASS |
| Catalog bug | UI must show board catalog when online | UI seeds/shows DEMO_CATALOG; board JSON correct | PASS (work correct — bug real) |
| Printer UX | Findable IP/port | On Target & Relays only; Tech has none; field report “still no ip and port” | PASS (work correct) |
| Waiting honest | Field checks need Jurien | RT-005/007/008 waiting | PASS |
| Wrong-repo items | None | RS485 RT-022/025/026 excluded | PASS |
| Dirty tree | Known | Untracked `data/config/` (WiFi secrets — do not commit); skills commit ahead | PASS (noted) |
| List access | LOCAL | `$HOME/.cursor/skills/rhynoTodoList/list.yaml` present | PASS |

**Audit verdict: PASS** — plan ready. **Gate OPEN** — Jurien approved 2026-08-09.

## Phases (build order)

### Phase 1 — Fix Product catalog LIVE vs demo (RT-033)
- **RT:** RT-033  
- **Scope:** Product page must not show Screw M6/Washer demo when board catalog exists. Load `/rest/liveWeightProducts` whenever authenticated + online; show clear **LIVE / DEMO** banner; surface save/load errors (no silent catch); never reset catalog to `DEMO_CATALOG` on remount if last load was live. After Save to catalog, list = board response.  
- **Acceptance:** Save `W3 Water` → leave Product → return → still `W3` (and other board products). Hard refresh still shows board list. Demo only when offline / explicit demo.  
- **KPI:** KPI-008  
- **Depends on:** —

### Phase 2 — Printer IP/port findable (RT-034)
- **RT:** RT-034 (reopens field intent of RT-017)  
- **Scope:** Keep settings on Target & Relays; add jump link from Tech (and optionally Live) “Printer IP & port → Target & Relays”; ensure printer card is **first or pinned near top** with Save; short helper text: not on Tech.  
- **Acceptance:** Jurien finds IP + port in under 30 seconds without hunting; can set IP, Save, enable print.  
- **KPI:** KPI-009  
- **Depends on:** —

### Phase 3 — Flash + field verify (RT-005, RT-007, RT-008)
- **RT:** RT-005, RT-007, RT-008  
- **Scope:** Flash after P1–P2; Jurien walks UI, DI Print/Next/Start/Stop, printer ticket.  
- **Acceptance:** Pass/fail noted per RT; waiting cleared only on her word.  
- **Depends on:** Phase 1–2

## Out of scope

- Option B / large catalog  
- Buzzer  
- MQTT off (RT-010 later)  
- WiFi report export (RT-019 later)  
- RS485-CanHatPi5DJB-W1X work (other repo)

## Improvements (KPI — required)

| KPI id | Improvement | Source RT / finding | Status | Sprint |
|--------|-------------|---------------------|--------|--------|
| KPI-008 | Product tab stuck on DEMO_CATALOG while board has real products | Field + API probe 2026-08-09 | shipped (code) | this |
| KPI-009 | Printer IP/port invisible when operator looks at Tech | Field report “still no ip and port” | shipped (code) | this |

## Pre-build gate

- [x] Data audit all PASS
- [x] Plan matches Option A / caps / dropped features
- [x] rhynoTodoList: RT-033/034 → planned → build → complete
- [x] Improvements section seeded
- [x] Jurien approved sprint (or explicit “build” / “go”)
- [x] No other-repo work

## Delivery notes

- Phase 1–2 implemented UI-only on `sprint-b/field-catalog-printer`
- Phase 3 field checks (RT-005 / RT-007 / RT-008) stay **waiting** on Jurien after flash
