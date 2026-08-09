# Operator QA smoke — Weighsoft.Hardware.Base

**Date:** 2026-08-09  
**Branch:** `sprint-e/qa-smoke-suite` → `RelayBoardEspBuildIn`  
**Board (lab):** `http://192.168.2.67` · env `esp12e` · MAC `E8:DB:84:97:CB:C0`  
**Credentials (lab default):** `admin` / `admin` — never commit other secrets

## Goal

Ship a **repeatable, PC-runnable** smoke suite that proves the relay board’s guest operator APIs, auth gates, and basic SPA reachability after flashes — without needing a full browser session for every check.

## Scope

| In scope | Out of scope |
|----------|----------------|
| HTTP reachability + REST guest/admin matrix | Firmware changes |
| Auth: guest vs JWT on config/catalog mutate | Flashing the board |
| Features flag `mqtt: false` when exposed | Physical DI wiring (RT-007) |
| Optional SPA HTML/JS 404 check | ESC/POS printer ticket (RT-008) |
| Human checklist for UI + DI + printer | New product features |

## Automated vs human-only

| ID | Mode | What |
|----|------|------|
| QA-001 … QA-012 | **Automated** | `scripts/qa/operator-smoke.ps1` |
| RT-007 | **Human** | DI Print / Next / Start / Stop (physical pin or Target Test buttons) |
| RT-008 | **Human** | Printer IP/port on Target & Relays + real ticket |
| UI live catalog | **Human** | Product tab shows LIVE Sand / Rock / Water (not DEMO) |
| Blank UI | **Browser** | Guest `/project/live-weight/live` must not be blank (PR #17). Script can only prove HTML/JS did not 404 |

Human checklist: [docs/QA-OPERATOR-CHECKLIST.md](../../QA-OPERATOR-CHECKLIST.md)

## Test matrix

| ID | Behaviour under test | Expect |
|----|----------------------|--------|
| QA-001 | Board reachable | HTTP root (or `/`) responds |
| QA-002 | Guest live weight | `GET /rest/liveWeight` → 200 |
| QA-003 | Guest product catalog | `GET /rest/liveWeightProducts` → 200, `count` ≥ 1 |
| QA-004 | Guest transactions | `GET /rest/liveWeightTransactions` → 200 |
| QA-005 | Guest weight update | `POST /rest/liveWeight` → 200 |
| QA-006 | Guest PLU select | `POST /rest/liveWeightProducts` `action=select` → 200 |
| QA-007 | Guest relay + status | `GET/POST /rest/relayBoard` 200; `GET /rest/relayBoardStatus` has `free_heap` |
| QA-008 | Config needs login | `GET /rest/liveWeightConfig` → 401 (no token) |
| QA-009 | Catalog upsert needs login | `POST` products `action=upsert` → 401 (no token) |
| QA-010 | Admin JWT | `POST /rest/signIn` → token; `GET /rest/liveWeightConfig` with Bearer → 200 |
| QA-011 | MQTT off | `GET /rest/features` → `mqtt` is `false` (skip if path missing) |
| QA-012 | SPA shell (optional) | `GET /project/live-weight/live` → 200; linked JS not 404. **Does not prove blank-screen fix** |

Product rules already shipped (suite asserts these):

- Guest: Live Weight, weigh/PLU select, transactions GET, DI/DO relay APIs
- Login: liveWeightConfig, catalog upsert/delete, WiFi/MQTT/Security, Target/Tech config paths
- `FT_MQTT=0`
- Catalog max 9; lab often has S1 Sand, R2 Rock, W3 Water
- Public UI guest-safe (LayoutMenu) — browser check, not fully automatable here

## How to run later

From repo root (PowerShell):

```powershell
.\scripts\qa\operator-smoke.ps1 -BaseUrl http://192.168.2.67
```

With credentials (defaults are lab admin/admin):

```powershell
.\scripts\qa\operator-smoke.ps1 -BaseUrl http://192.168.2.67 -User admin -Password admin
```

Exit code **0** = all required checks PASS; **non-zero** = at least one FAIL.  
Use after every firmware flash or WiFi recovery to re-verify the board before handing off RT-007 / RT-008.

## Review notes (this pack)

- Script-only PR; no firmware flash required.
- If the board is offline during CI/agent run: still merge the suite; document “board unreachable” and re-run when LAN is up.
- Never commit `data/config/wifiSettings.json` or passwords beyond lab default docs.
