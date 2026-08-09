# Sprint: Heap + field bugs (agent browser findings)

**Created:** 2026-08-09  
**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Skill:** RhynoSprintPlanCreate  
**Architecture plan:** `docs/superpowers/plans/2026-08-09-esp8266-live-weight-architecture-options.md`  
**Evidence:** Agent Playwright test 2026-08-09 ~18:50; training log field outcomes  
**Sprint id:** `SPRINT-2026-08-09-HWB-E`  
**KPI log:** `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`  
**Status:** shipped (branch `sprint-e/heap-public-operator`)  
**List mode:** LOCAL LIST MODE

## Goal

Stop the board running out of memory, fix the **broken printer link**, fix **catalog / active PLU** under load, then flash and re-test.

## Why this sprint

Agent browser test proved:

1. Twin shows **~2.4 KB free RAM** → REST catalog **CONNECTION_RESET**, SPA JS **CONTENT_LENGTH_MISMATCH**
2. Active PLU stuck on demo **Screw M6** while `products.json` has **S1/R2/W3**
3. Tech printer deep-link `/live-weight/target` lands on the **wrong page**
4. Discovery UI and job-wide count **passed**

## Data audit

| Check | Expected | Found | Result |
|-------|----------|-------|--------|
| Repo | Weighsoft.Hardware.Base | Workspace matches | PASS |
| Branch | RelayBoardEspBuildIn | `git branch` matches | PASS |
| Product option | Option A, 9/40, no buzzer, 2 DI | Unchanged | PASS |
| Caps | 9 products / 40 tx | Still enforced | PASS |
| Heap issue real | Twin stress | Free RAM 2.4 KB; catalog/JS fail | PASS (work correct) |
| Waiting honest | DI + printer ticket need Jurien | RT-007 / RT-008 ticket still human | PASS |
| Wrong-repo | None | RS485 excluded | PASS |
| Dirty tree | Known | `data/config/` secrets untracked; scripts untracked | PASS (noted) |
| List access | LOCAL | `$HOME/.cursor/skills/rhynoTodoList/` | PASS |

**Audit verdict: PASS** — gate blocked on Jurien approve / build.  
**Decision needed:** RT-060 — Live Weight as post-login home? (recommended **yes** with RT-057)

## Phases (build order)

### Phase 1 — Fix Tech printer deep-link (RT-056)
- **Scope:** Change Tech (and any) links from `/live-weight/target#network-printer` to `/project/live-weight/target#network-printer` (or React Router `to` that resolves correctly).
- **Acceptance:** Click from Tech opens Target & Relays Network printer card (not Twin).
- **KPI:** KPI-021  
- **Depends on:** —

### Phase 2 — Heap relief (RT-054 + pull RT-010)
- **Scope:** Reduce ESP8266 RAM pressure: optional **MQTT off** by default / feature flag; avoid loading Twin + Live Weight WS together if possible; document heap budget. Prefer compile-time or settings default that recovers free heap above ~12 KB with Live Weight open.
- **Acceptance:** With Live Weight open (not Twin), free heap stays usable; Product catalog GET succeeds from browser without CONNECTION_RESET in 3 of 3 tries.
- **KPI:** KPI-020 (ship when heap usable)  
- **Depends on:** —

### Phase 3 — Catalog + active PLU sync (RT-055 + RT-059)
- **Scope:** On LIVE catalog load success, do not show demo Screw M6; sync active PLU from board state or first catalog entry; cache last-good catalog on REST fail (stale-while-revalidate); clear error + Retry.
- **Acceptance:** Browser Product shows S1/R2/W3 (or current board list); leave tab and return still shows board list when heap OK; fail shows error without wiping to empty demo forever.
- **Depends on:** Phase 2 preferred (heap)

### Phase 4 — Default landing (RT-057 / RT-060)
- **Scope:** If Jurien says **yes** to RT-060: post-login / default project route → Live Weight (not Twin). Twin remains in menu.
- **Acceptance:** Fresh login lands on Live Weight; Twin still reachable.
- **Depends on:** Jurien answer on RT-060 (default recommend **yes**)

### Phase 5 — Flash + re-test
- **Scope:** Flash once after P1–P4; agent re-runs browser checks; Jurien still owns physical DI + printer ticket (RT-007 / RT-008).
- **Acceptance:** Catalog load PASS in browser; printer link PASS; heap note recorded; RT-043 re-checked.
- **Depends on:** P1–P4

## Out of scope

- RT-058 heap warn banner (later)
- RT-040 per-PLU count
- RT-019 WiFi report export
- RS485-CanHatPi5DJB-W1X
- Option B

## Improvements (KPI — required)

| KPI id | Improvement | Source | Status | Sprint |
|--------|-------------|--------|--------|--------|
| KPI-020 | Heap starvation found in agent browser test | RT-054 | shipped | this |
| KPI-021 | Fix Tech printer deep-link `/project` prefix | RT-056 | shipped | this |
| KPI-022 | Catalog last-good cache + no demo wipe | RT-055/059 | shipped | this |
| KPI-023 | Default landing Live Weight (if approved) | RT-057/060 | shipped | this |

## Pre-build gate

- [x] Data audit all PASS
- [x] Plan matches Option A / caps / dropped features
- [x] rhynoTodoList items → planned then complete (RT-010/054–057/059/060)
- [x] Improvements section seeded
- [x] Jurien approved / said build
- [x] No other-repo work
- [x] RT-060 answered **yes** — Live Weight home + public operator mode

## Jurien decisions (locked)

1. **Approve Sprint E** — yes  
2. **RT-060:** Login home = **Live Weight** (yes)  
3. Public operator: weigh / PLU / tx / DI-DO without login; settings need login  
4. Build + flash after merge
