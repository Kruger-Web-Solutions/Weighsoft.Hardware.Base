# Architecture options — Live Weight / WiFi / serial / storage (ESP8266)

**Date:** 2026-08-09  
**Branch:** `RelayBoardEspBuildIn`  
**Board IP (field):** `192.168.2.67`  
**Mode:** research + expert swarm → **two options for Jurien** (not building yet)

## Research sources

- ESP8266 Arduino filesystem / LittleFS docs  
- esp8266.com forums (WiFi reconnect, AP+STA channel pitfalls)  
- GitHub esp8266/Arduino issues (LittleFS rewrite cost, WiFi.begin spam)  
- Arduino Forum / HA community (RS-232 scale parse patterns)  
- Expert briefs: storage, WiFi/AsyncWebServer, serial ingest (this session)

## Locked product facts

- **2 DIs only** (DI3/DI4 dropped — RT-013)  
- **Buzzer dropped** — no firmware / UI / plan work  
- No onboard temp / power-usage chip  
- Remove LED Example (RT-012)  
- Printer IP on Target & Relays (clarify UX)

---

## Shared core (both options use this)

| Area | Design |
|------|--------|
| Serial | Drain UART every loop → 128 B line buffer → simple numeric parse → stable filter → state publish **≤5 Hz**, change-gated |
| Regex | Optional Tech tool; **compile once** or prefer simple parse in production |
| WiFi | STA + auto-reconnect; SoftAP only when provisioning / long fail; avoid permanent AP+STA while weighing |
| WS | Live Weight **Connect/Stop**; drop-if-busy; ~5–10 Hz max; prefer 1 stream client |
| Side I/O | Printer TCP / blocking work **only in loop()**, never in HTTP/WS callback |
| Config save | Rewrite JSON only on **Save** (wifi, liveWeight, products) — never every weight tick |
| Weigh history | Append-only **NDJSON** under `/log/` if logging needed |
| Reports | Authenticated **chunked file stream** from LittleFS — never load whole file into RAM |
| PLU catalog | Soft max **~80**, hard **~100** if loaded as one JSON |

---

## Option A — “Lean field board” (recommended for this ESP8266)

**Goal:** Rock-stable weighing + relays + DI + print. Keep catalog small. Defer heavy reports.

### Functions

1. Remove LED Example  
2. Harden serial ingest (shared core)  
3. Twin: live WS, click relays, DI lights  
4. Tech: Connect/Stop + stream box (WS or REST poll of `last_line`)  
5. Clear printer IP/port UI  
6. Single active PLU + optional **small** products.json (caps: 9 products / 40 tx)  
7. Print ticket + optional append one NDJSON line per print/next  
8. OTA IP update; MQTT off  
9. Docs: no temp/power meter; no buzzer

### Defer

- Big multi-PLU CRM  
- Fancy PDF reports  
- DI3/DI4  

### Pros / cons

- **Pros:** Fits 16–23 KB heap; matches forum/ESP docs; ships field checks faster  
- **Cons:** Catalog not huge; reports = download NDJSON/CSV later  

### Sprint order

RT-012 → RT-020 → RT-014/015 → RT-016 → RT-017 → RT-009/010 → RT-021 → RT-018 (9+40) → RT-005/007/008 verify  

---

## Option B — “Catalog + report board”

**Goal:** Same stability core, plus **product catalog** and **WiFi report download** as first-class.

### Functions

Everything in Option A, plus:

1. `products.json` with add/edit/delete UI (hard cap 100)  
2. Active PLU picker on Product / Live strip  
3. `/log/YYYYMMDD.ndjson` append on completed weigh / print  
4. Authed `GET /rest/liveWeightReport` streams log (NDJSON or line-CSV)  
5. FS free-space guard; delete oldest log day when full  

### Pros / cons

- **Pros:** Answers “many PLUs” + “report over WiFi” in one sprint family  
- **Cons:** More flash wear risk if mis-tuned; more UI; longer to verify; still not thousands of PLUs  

### Sprint order

Option A stability first (012, 020, twin, Connect), **then** RT-018 + RT-019 before calling done  

---

## What we would miss if we only polish UI

- Serial still able to WDT under continuous scale  
- Twin stuck in Demo when WS fails  
- Saving every weight sample to JSON (flash death)  
- Blocking print inside HTTP handlers  

---

## Decision (2026-08-09)

**Chosen: Option A — Lean field board**

Field caps (fits ESP8266 easily):

| Store | Max entries | Notes |
|-------|-------------|--------|
| Products / PLU | **9** | `products.json` rewrite on Save |
| Transactions | **40** | NDJSON or ring file; drop oldest when full |

**Buzzer:** stripped from code and plan (2026-08-09). Not shipping.

Next: move RT items to **planned** / **build** on request.
