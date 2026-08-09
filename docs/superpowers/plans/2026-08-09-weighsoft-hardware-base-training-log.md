# Weighsoft.Hardware.Base — training / decision log

**Repo:** `Weighsoft.Hardware.Base`  
**Branch:** `RelayBoardEspBuildIn`  
**Purpose:** Chronological decisions + field outcomes for future training data and KPI.  
**Updated:** 2026-08-09

## Decisions (chronological)

| When | Decision | Source | Durable refs |
|------|----------|--------|--------------|
| 2026-08-09 | Option A lean field board; 9 products / 40 tx | Jurien | architecture options plan; RT-023 |
| 2026-08-09 | Drop buzzer; strip code | Jurien | RT-006 / RT-024 |
| 2026-08-09 | Stay at 2 DIs only | Jurien | RT-013 |
| 2026-08-09 | Sprint A Option A software shipped | agents | SPRINT-2026-08-09-HWB-A |
| 2026-08-09 | Sprint B LIVE catalog + printer findable | field + agents | SPRINT-2026-08-09-HWB-B; RT-033/034 |
| 2026-08-09 | Discovery Option 2; sender = any LAN device | Jurien | Sprint C; RT-039; WIFI-WEIGHT-DISCOVERY.md |
| 2026-08-09 | Per-PLU count discussed, **not** approved → later | Jurien | RT-040; KPI-016 |
| 2026-08-09 | Count stays **job-wide** until RT-040 | Jurien | questions Q-005; TEST RT-045 |
| 2026-08-09 | Printer on Target & Relays, not Tech | Jurien / field | RT-017/034; Q-006 |
| 2026-08-09 | Keep going on software until cleared; **flash after** software clear; then she tests | Jurien ~18:25 | SPRINT-2026-08-09-HWB-D |
| 2026-08-09 | Create test list + questions section; save for training/KPI | Jurien | living list ### tests / ### questions |
| 2026-08-09 | Do not mix RS485-CanHatPi5DJB-W1X into HWB sprint | Jurien / process | sprint D out of scope |
| 2026-08-09 | Never commit `data/config/wifiSettings.json` | standing rule | dirty-tree audits |
| 2026-08-09 | **Approved x2** — Sprint D + flash now | Jurien | RT-046; HWB-D gate |
| 2026-08-09 | Flash delivered via HTTP `/rest/uploadFirmware` to 192.168.2.67 | agent | sketch 915072; discovery REST OK |
| 2026-08-09 | Agent browser field test; heap starvation + broken printer deep-link + catalog fail under load | agent Playwright | RT-054…060; training field outcomes |
| 2026-08-09 | **Approved Sprint E** + RT-060 **yes** + public operator Live Weight | Jurien | SPRINT-2026-08-09-HWB-E |
| 2026-08-09 | Sprint E shipped: FT_MQTT=0, public weigh/PLU/DI-DO, config auth, PLU sync, printer deep-link, Live Weight landing | agent | branch `sprint-e/heap-public-operator`; KPI-020…023 |
| 2026-08-09 | Sprint E flash: HTTP OTA to 192.168.2.67 failed (host unreachable); **USB COM4** upload SUCCESS (hash verified, MAC e8:db:84:97:cb:c0). Post-flash WiFi 192.168.2.67 not yet responding — needs Jurien power-cycle / WiFi check | agent | PR #16 merged |

## Software clear snapshot (HWB-D audit)

- Open software planned/build for this repo: **none**  
- Later parked: RT-010, RT-019, RT-040  
- Waiting field tests: RT-005, RT-007, RT-008, RT-043, RT-044, RT-045  
- Flash: RT-046 **complete** — HTTP flash 2026-08-09 ~18:35; hand off tests

## Field outcomes

| RT | Test | Result | When | Notes |
|----|------|--------|------|-------|
| RT-044 | Discovery How senders find me | **PASS** (agent) | 2026-08-09 | Tech UI + discovery REST OK |
| RT-045 | Count job-wide | **PASS** (agent) | 2026-08-09 | 1.50 × count 3 → total 4.500 |
| RT-043 | Product catalog LIVE persist | **FAIL** (agent) | 2026-08-09 | Browser CONNECTION_RESET; REST OK when heap free; see RT-054/055/059 |
| RT-008 | Printer IP/port + ticket | **PARTIAL** (agent) | 2026-08-09 | Fields on Target OK; Tech deep-link broken RT-056; ticket needs real printer |
| RT-005 | Live Weight UI overall | **PARTIAL** (agent) | 2026-08-09 | Live 1.50 OK; heap/JS stress; Jurien still confirm |
| RT-007 | DI Print/Next/Start/Stop | **pending human** | | Twin maps DI1=next DI2=start; physical DI not pressed |
| RT-008 | Printer ticket | **pending human** | | Need real ESC/POS printer on LAN |

## Agent browser session (2026-08-09 ~18:50)

- Method: Playwright headed → `http://192.168.2.67` admin/admin  
- Twin LIVE: Free RAM **2.4 KB**, frag 39%  
- Product: catalog REST **ERR_CONNECTION_RESET** in browser; PC REST OK when heap ~11 KB  
- Active PLU **Screw M6** vs catalog **S1/R2/W3**  
- Tech printer link `/live-weight/target` → wrong page (relay twin)  
- Live weight POST 1.50 → UI Net/Gross **1.50 kg**  
- New list ids: RT-054…RT-060 (issues/ideas/Q)

## KPI pointers

- Log: `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`  
- Sprint D: `SPRINT-2026-08-09-HWB-D`  
- Improvements: KPI-017…019; agent-test issues KPI-020+
