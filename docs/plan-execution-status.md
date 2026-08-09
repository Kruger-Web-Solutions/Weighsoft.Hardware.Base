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

## Sprint C — WiFi weight discovery (draft — gate not open)

**Plan:** `docs/superpowers/plans/2026-08-09-weighsoft-hardware-base-discovery-sprint.md`  
**Sprint id:** `SPRINT-2026-08-09-HWB-C`  
**Status:** **draft** — data audit PASS; waiting Jurien **approve / build**  
**List:** RT-039 todo → planned (LOCAL); RT-005/007/008 stay waiting; RT-040 per-PLU count later (not approved)

| Phase | State | RT | Notes |
|-------|-------|-----|-------|
| P1 Spec / protocol + sender doc | planned | RT-039 | UDP and/or mDNS; document for any LAN sender |
| P2 Board announce | planned | RT-039 | Lean ESP8266 announce when STA up |
| P3 Tech/UI identity + find-me | planned | RT-039 | Manual IP is sender-side |
| P4 Reference sender / harness | planned | RT-039 | Keep lean |
| P5 Field verify + carry checks | waiting (human) | RT-039 + RT-005/007/008 | Discovery field + prior field checks |

**KPI:** KPI-014 attached; KPI-015 heap lean; KPI-016 / RT-040 per-PLU parked later  

**Out of scope:** RS485-CanHatPi5DJB-W1X; per-PLU count build; no flash/code until gate PASS  
