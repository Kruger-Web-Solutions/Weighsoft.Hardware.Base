# Phase 1 plan — Firmware contract

**Goal:** DI actions, network ESC/POS print, job start/stop, serial churn reduced. (Buzzer dropped from product.)  
**Branch:** `feat/lw-p1-firmware-di-print`  
**Acceptance:** see `docs/superpowers/plans/2026-08-08-live-weight-operator-ui-phases.md` §P1

## Tasks
1. Extend `LiveWeightState` JSON/config with DI/printer/job fields
2. `LiveWeightService`: `onDiEdge`, `runAction`, `sendPrintTicket`, serial only CHANGED when weight/line changes
3. `RelayBoardService`: rising-edge DI callback
4. `main.cpp`: wire callback
5. Build `esp12e` + format
