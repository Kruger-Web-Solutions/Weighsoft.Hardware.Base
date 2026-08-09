# Operator QA checklist (human)

Use after a flash or when the automated smoke suite (`scripts/qa/operator-smoke.ps1`) has passed.

**Board (lab):** http://192.168.2.67 · login `admin` / `admin` when needed

## Before you start

1. Run automated smoke:

   ```powershell
   .\scripts\qa\operator-smoke.ps1 -BaseUrl http://192.168.2.67
   ```

2. Confirm PASS summary (QA-001 … QA-012 as applicable).

## Browser / UI

- [ ] Open http://192.168.2.67/project/live-weight/live **logged out** — page is **not blank** (shows Live Weight UI).
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
