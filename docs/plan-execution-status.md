# Plan execution status — Hardware.Base

**Base:** `RelayBoardEspBuildIn`  
**Repo:** `Weighsoft.Hardware.Base`  
**Updated:** 2026-08-09  

## Sprint A — Option A (closed — code shipped)

**Plan:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-option-a-sprint.md`  
**Sprint id:** `SPRINT-2026-08-09-HWB-A`  
P1–P7 **DELIVERED** (PR #6–#10). Flash done (USB COM4). Field checks carried to Sprint B.

## Sprint B — Field fix (P1–P2 delivered + flashed)

**Plan:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-field-fix-sprint.md`  
**Sprint id:** `SPRINT-2026-08-09-HWB-B`  
**PR:** https://github.com/Kruger-Web-Solutions/Weighsoft.Hardware.Base/pull/11 (merged)  
**Status:** P1–P2 code + flash done; P3 field checks waiting on Jurien  

| Phase | State | RT | Notes |
|-------|-------|-----|-------|
| P1 Catalog LIVE UI | delivered | RT-033 | LIVE board catalog; no DEMO wipe when online |
| P2 Printer findable | delivered | RT-034 | Tech jump + printer card pinned on Target & Relays |
| P3 Field verify | **needs:human** | RT-005/007/008 | Jurien: UI look, DI Print/Next/Start/Stop, printer ticket |

**Flash:** ArduinoOTA failed (OOM / no callback). USB COM4 not present. **HTTP `/rest/uploadFirmware` succeeded** to `192.168.2.67` (sketch 912000). Post-flash REST still shows S1/R2/W3.  

**KPI:** KPI-008, KPI-009 **shipped**  

## Sprint C — WiFi weight discovery (P1–P4 delivered; P5 human)

**Plan:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-discovery-sprint.md`  
**Sprint id:** `SPRINT-2026-08-09-HWB-C`  
**Status:** **in_build → P1–P4 DELIVERED**; P5 **needs:human**  
**Combine note:** P1–P4 shipped as one feature PR (#12) then UDP/REST hot-fixes (#13–#15) — same protocol contract; deps respected.  
**List:** RT-039 **complete** (announce + docs + UI + harness); RT-005/007/008 stay **waiting**; RT-040 later  

| Phase | State | RT | Notes |
|-------|-------|-----|-------|
| P1 Spec / protocol + sender doc | **DELIVERED** | RT-039 | `docs/WIFI-WEIGHT-DISCOVERY.md` — UDP primary, mDNS, REST helper, sender-side manual IP |
| P2 Board announce | **DELIVERED** | RT-039 | `LiveWeightDiscovery` UDP :4210 / local :4211 + mDNS `_weighsoft-lw._tcp` |
| P3 Tech/UI identity + find-me | **DELIVERED** | RT-039 | Tech **How senders find me**; no sender-IP box on board |
| P4 Reference sender / harness | **DELIVERED** | RT-039 | `scripts/listen-weighsoft-announce.py` (+ `--rest`) |
| P5 Field verify + carry checks | **needs:human** | RT-039 field + RT-005/007/008 | Desk UDP hear + prior field checks |

**PRs:**  
- https://github.com/Kruger-Web-Solutions/Weighsoft.Hardware.Base/pull/12 (merged) — P1–P4 feature  
- https://github.com/Kruger-Web-Solutions/Weighsoft.Hardware.Base/pull/13 (merged) — ESP8266 UDP bind fix  
- https://github.com/Kruger-Web-Solutions/Weighsoft.Hardware.Base/pull/14 (merged) — REST discovery + unicast poke  
- https://github.com/Kruger-Web-Solutions/Weighsoft.Hardware.Base/pull/15 (merged) — REST JSON fields fix  

**Flash:** HTTP `/rest/uploadFirmware` to `192.168.2.67` succeeded (sketch ~915072). Prolific COM4 present but unused (HTTP preferred).  

**Desk verify:** `GET /rest/liveWeightDiscovery` → `udp_ready=true`, `last_send_ok=true`, `unicast_to_client_ok=true`, host `esp8266-relayboard`, ip `192.168.2.67`. Test weight POST `1.25` OK. PC UDP listen on :4210 heard nothing (likely Windows inbound firewall / AP filter) — Jurien field check should try phone/Pi or allow UDP 4210. Heap free ~17 KB after flash.  

**KPI:** KPI-014, KPI-015 **shipped**; KPI-016 / RT-040 parked later  

**Out of scope held:** RS485 other repo; per-PLU count  

## Sprint D — Software clear → flash → HUMAN tests

**Plan:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-software-clear-sprint.md`  
**Sprint id:** `SPRINT-2026-08-09-HWB-D`  
**Status:** Software clear done; **flash DELIVERED**; field tests **needs:human**  
**Gate:** Jurien **approved x2** (sprint D + flash) 2026-08-09 ~18:30  

| Phase | State | RT | Notes |
|-------|-------|-----|-------|
| P1 Software inventory | **DELIVERED** | audit | No remaining planned/build software RTs |
| P2 Doc gaps | **DELIVERED** | KPI-019 | UDP 4210 firewall note + job-wide count |
| P3 Flash | **DELIVERED** | RT-046 | HTTP `/rest/uploadFirmware` to `192.168.2.67` |
| P4 HUMAN test list | **needs:human** | RT-005/007/008/043/044/045 | Waiting on Jurien |
| P5 Questions + training | **DELIVERED** | RT-047… | Store + training log + KPI-017…019 |

**Flash:** Existing `esp12e` `firmware.bin` (915072, built 2026-08-09 17:57 from `780825e`). Kill Python; JWT `signIn`; HTTP POST `/rest/uploadFirmware` → **200**. Post-flash: ping OK, HTTP root **200**, `GET /rest/liveWeightDiscovery` → `udp_ready=true`, `last_send_ok=true`, `unicast_to_client_ok=true`, host `esp8266-relayboard`, id `97cbc0`, heap free ~16.5 KB. WiFi secrets not committed.

**Next:** Jurien starts test list **RT-005 / RT-007 / RT-008 / RT-043 / RT-044 / RT-045**.

**KPI:** KPI-017, KPI-018, KPI-019 **shipped** (bookkeeping); flash RT-046 complete  

