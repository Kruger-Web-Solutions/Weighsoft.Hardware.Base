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

## Software clear snapshot (HWB-D audit)

- Open software planned/build for this repo: **none**  
- Later parked: RT-010, RT-019, RT-040  
- Waiting field tests: RT-005, RT-007, RT-008, RT-043, RT-044, RT-045  
- Flash: RT-046 **complete** — HTTP flash 2026-08-09 ~18:35; hand off tests

## Field outcomes (placeholders — fill after Jurien tests)

| RT | Test | Result | When | Notes |
|----|------|--------|------|-------|
| RT-043 | Product catalog LIVE persist | _pending_ | | Leave tab / refresh |
| RT-008 | Printer IP/port + ticket | _pending_ | | Target & Relays; ESC/POS |
| RT-007 | DI Print / Next / Start / Stop | _pending_ | | Pin to GND |
| RT-005 | Live Weight UI overall | _pending_ | | Dial / tabs / zones |
| RT-044 | Discovery find-me + listen script | _pending_ | | UDP 4210; firewall? |
| RT-045 | Count job-wide (until RT-040) | _pending_ | | Shared count expected |

## KPI pointers

- Log: `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`  
- Sprint D: `SPRINT-2026-08-09-HWB-D`  
- Improvements seeded: KPI-017 (test+questions list), KPI-018 (this training log), KPI-019 (UDP 4210 firewall note)
