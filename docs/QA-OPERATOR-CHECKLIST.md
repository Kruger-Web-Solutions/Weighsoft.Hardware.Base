# Operator QA checklist (human)

Use after a flash or when the automated smoke suite (`scripts/qa/operator-smoke.ps1`) has passed.

**Board (lab):** http://esp8266-relayboard.local · login `admin` / `admin` when needed

Use the **name**, not an IP address. The router hands the board a different IP over time
(it has been `.3.117`, `.2.67`, `.2.55`). A stale IP is worse than a dead one: another
device can take that address, answer ping, and serve nothing — which reads as a crashed
board. The board's own id is **`97cbc0`** (last 6 of its MAC) and never changes.

## Before you start

1. Run automated smoke:

   ```powershell
   .\scripts\qa\operator-smoke.ps1
   ```

2. Confirm PASS summary (QA-000 … QA-012 as applicable).
   **QA-000 is the identity check** — it fails if something other than our board answers.

### If the board seems dead

- [ ] Does the name resolve? `ping esp8266-relayboard.local` — the reply shows the current IP.
- [ ] Check it is really ours: open `/rest/liveWeightDiscovery` and confirm `"id": "97cbc0"`.
- [ ] A different `id`, or ping working while pages refuse, means **the address moved** — do
      not power-cycle, just use the name.

## Browser / UI

- [ ] Open http://esp8266-relayboard.local/project/live-weight/live **logged out** — page is **not blank** (shows Live Weight UI).
- [ ] Product tab shows **LIVE** catalog: Sand / Rock / Water (or current board products) — **not DEMO**.
- [ ] Dial / weight area updates when weight is posted or scale sends data.

## RT-007 — DI Print / Next / Start / Stop (physical)

- [ ] On Target & Relays: DI1 / DI2 actions mapped (lab often DI1=next, DI2=start).
- [ ] Close DI pin to GND **or** use Target **Test** Print / Next buttons.
- [ ] Confirm count increments and/or print/status events as mapped.
- [ ] Twin (if used) shows DI active when pin is closed.

## RT-008 — Printer ticket

- [ ] Find printer **IP** and **port** on **Target & Relays** (not Tech) in under ~30s.
- [ ] Enable network print; set printer IP/port.
- [ ] Trigger Print or DI Print; confirm ESC/POS ticket on the printer.

## Sign-off

| Check | Who | Date | Result |
|-------|-----|------|--------|
| Smoke script | | | |
| Guest UI not blank | | | |
| LIVE catalog | | | |
| RT-007 DI | | | |
| RT-008 printer | | | |
