# Sprint: WiFi weight discovery (board announce)

**Created:** 2026-08-09  
**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Skill:** RhynoSprintPlanCreate  
**Architecture plan:** `docs/superpowers/plans/2026-08-09-esp8266-live-weight-architecture-options.md`  
**Prior sprints:**  
- `SPRINT-2026-08-09-HWB-A` — Option A lean field board (P1–P7 closed)  
- `SPRINT-2026-08-09-HWB-B` — field fix catalog + printer (PR #11 merged + flash done; field checks still open)  
**Sprint id:** `SPRINT-2026-08-09-HWB-C`  
**KPI log:** `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`  
**Status:** P1–P4 delivered (PR #12–#15); P5 needs:human  
**List mode:** LOCAL LIST MODE (`$HOME/.cursor/skills/rhynoTodoList/`)

## Goal

Board **announces itself on the LAN** so **any sender** can auto-find and push weight **to** the board; keep a **manual IP** path on the sender when auto-find fails.

## Locked product decisions (this sprint)

- **Option 2 discovery** locked 2026-08-09 (architecture plan).  
- **Sender = any LAN device** (ESP, WOW Trade / Pi, PC, or future) — not one fixed product.  
- Protocol must be **simple and documentable** (UDP broadcast and/or mDNS).  
- Data direction unchanged: sender **pushes weight TO** the relay board (board does not pull).  
- Jurien invoked `/RhynoSprintPlanCreate` → **RT-039 pulled into this sprint** (todo → planned).  
- **Per-PLU count** was discussed but **not approved** → parked as later **RT-040** / KPI seed only (not in build phases).

## Data audit

| Check | Expected | Found | Result |
|-------|----------|-------|--------|
| Repo | `Weighsoft.Hardware.Base` | Workspace folder matches; one-repo sprint only | PASS |
| Branch | `RelayBoardEspBuildIn` | `git branch --show-current` = `RelayBoardEspBuildIn` | PASS |
| Product option | Option A lean board + Option 2 discovery locked | Architecture plan: Option A caps; discovery Option 2 locked; sender = any LAN device | PASS |
| Caps | 9 products / 40 tx still hold | Unchanged from Option A; discovery does not raise caps | PASS |
| Dropped features | No buzzer; no DI3/DI4 | Still stripped / decision complete (RT-006/024/013) | PASS |
| Waiting honest | RT-005/007/008 still need Jurien | Still `waiting` on list; Sprint B P3 not fake-complete | PASS |
| Wrong-repo items | None from RS485 / other products | Sprint RTs are HWB only; RS485 stays out | PASS |
| Dirty tree known | Secrets + prior doc/KPI dirt noted | See dirty tree audit below | PASS (noted) |
| List access | LOCAL LIST MODE | `$HOME/.cursor/skills/rhynoTodoList/list.yaml` present | PASS |
| Prior delivery | Sprint A closed; Sprint B PR #11 + flash | A P1–P7 done; B merge `0009b45` + HTTP flash; HEAD docs flash note `4be61ff` | PASS |

### Dirty tree audit (at plan write)

| Path | Note | Commit? |
|------|------|---------|
| `data/config/` (e.g. `wifiSettings.json`) | Likely WiFi secrets from board FS | **Never commit** |
| `.claude/skills/RhynoSprintPlanCreate/kpi.yaml` | KPI dirt from prior + this sprint | Docs/KPI OK to commit |
| `docs/superpowers/plans/2026-08-09-esp8266-live-weight-architecture-options.md` | Discovery decision text | OK |
| `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-field-fix-sprint.md` | Sprint B notes | OK |
| `scripts/verify-rhyno-skills-clone.sh` | Unrelated untracked script | Out of this sprint commit unless wanted |

**Audit verdict: PASS** — plan ready for Jurien approve. **Gate NOT open for build** until she says approve / build.

## Phases (build order)

### Phase 1 — Spec / protocol for announce + sender doc
- **RT:** RT-039 (spec slice)  
- **Scope:** Choose and document announce method (UDP broadcast and/or mDNS). Define packet / service fields: board id, IP, port for weight push, maybe hostname. Write a short **sender-facing** doc so any LAN device can implement listen + auto-adopt + manual IP fallback. Confirm push endpoint the board already expects (or minimal addition) without pulling weight.  
- **Acceptance:** One docs page a sender author can follow; protocol choice explicit; manual IP described as **sender-side** only.  
- **KPI:** KPI-014  
- **Depends on:** —

### Phase 2 — Board announce implementation
- **RT:** RT-039 (firmware slice)  
- **Scope:** When STA is up, board announces on LAN per Phase 1. Keep lean for ESP8266 heap (small payload, sensible interval, no chatty loops). No RS485 / other-repo code.  
- **Acceptance:** On Page Home WiFi, a LAN listener sees the announce (or mDNS name) and can resolve board IP. Heap stays stable under normal Live Weight use.  
- **KPI:** KPI-014, KPI-015  
- **Depends on:** Phase 1

### Phase 3 — Tech / UI: board identity + how senders find me
- **RT:** RT-039 (UI slice)  
- **Scope:** Tech (or Live Weight tech strip) shows board identity useful for discovery (IP, hostname / service name, short “how senders find me”). Note clearly: **manual IP is configured on the sender**, not as a second discovery server on the board.  
- **Acceptance:** Operator can read board identity and the find-me story in under 30 seconds; no fake “type sender IP here” for discovery.  
- **KPI:** KPI-014  
- **Depends on:** Phase 1 (Phase 2 preferred so UI matches live announce)

### Phase 4 — Minimal reference sender or test harness note
- **RT:** RT-039 (verify harness)  
- **Scope:** Keep lean — either a tiny ESP/PC reference listener that auto-adopts + optional manual IP, **or** a documented test harness (script / steps) that proves announce without a full product UI. Do not ship a heavy second app that hurts ESP8266 heap on the board.  
- **Acceptance:** Agent or Jurien can prove auto-find on the desk LAN; manual IP path documented if auto-find is skipped.  
- **KPI:** KPI-015  
- **Depends on:** Phase 2

### Phase 5 — Field verify discovery + carry field checks
- **RT:** RT-039 (field), RT-005, RT-007, RT-008  
- **Scope:** Flash/discovery verify on real board after P1–P4. Carry Sprint B field checks as **needs:human** — still waiting; do not mark complete without Jurien.  
- **Acceptance:**  
  - Discovery: sender finds board (or manual IP works) and can push a test weight.  
  - RT-005 / RT-007 / RT-008: pass/fail only on Jurien’s word.  
- **Depends on:** Phase 2–4 for discovery; field checks independent of discovery code quality  
- **Stage note:** RT-005/007/008 remain `waiting` until she tests.

## Out of scope

- **Per-PLU count** UI/firmware (discussed, not approved) — **RT-040** later only  
- Option B large catalog / WiFi report export (RT-019 later)  
- MQTT off heap cleanup (RT-010 later)  
- Buzzer / DI3 / DI4  
- Any work in `RS485-CanHatPi5DJB-W1X` or other product repos  
- Building / flashing before gate PASS + Jurien go

## Improvements (KPI — required)

Seed before build. Agents add rows when they find more.

| KPI id | Improvement | Source RT / finding | Status | Sprint |
|--------|-------------|---------------------|--------|--------|
| KPI-014 | WiFi weight discovery — board announce + sender auto-adopt + manual IP | RT-039; Option 2 locked | shipped | HWB-C |
| KPI-015 | Keep announce payload + interval lean for ESP8266 heap | RT-039 / ESP8266 risk | shipped | HWB-C |
| KPI-016 | Per-PLU count (discussed, not approved) | Chat 2026-08-09 | open / later | parked RT-040 |

## Pre-build gate

- [x] Data audit all PASS (or FAIL fixed + re-audited)
- [x] Plan doc matches Option / caps / dropped features
- [x] Sprint items planned in rhynoTodoList for this repo (LOCAL) OR proposed for PC apply (remote) and Jurien accepts that
- [x] Improvements section exists (at least seed KPIs)
- [x] Jurien approved sprint (or explicit "build" / "go") — `/weighsoft-plan-execute` gate OPEN
- [x] No work from other repos in this sprint

## Jurien decisions needed

1. **Approve** Sprint C (`SPRINT-2026-08-09-HWB-C`) and say **build** — or change scope first.  
2. Confirm UDP vs mDNS preference if she has one (otherwise agents pick simplest lean path in Phase 1 and document it).  
3. Field checks RT-005 / RT-007 / RT-008 still waiting on her desk tests (unchanged).  
4. Per-PLU count stays **later** unless she explicitly pulls RT-040 into a sprint.
