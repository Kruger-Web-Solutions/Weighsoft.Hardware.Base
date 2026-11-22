# Resilience and Discovery — what's running on the devices today

**Date:** 2026-05-03
**Branch:** `SerialReaderWriter`
**Status:** Implemented — all three test devices (`.53`, `.60`, `.64`) at commit `b7e9ae8`
**Audience:** anyone returning to this codebase to extend or debug the resilience features

This document is the "as-built" companion to the plan at `~/.claude/plans/please-remove-this-diagnostics-vast-sparrow.md`. It describes how the device recovers from common failures, what the dashboard shows during those failures, and where the still-deferred work sits.

---

## 1. The big picture

Three things keep a device usable when something goes wrong:

1. **Two layers of watchdog.** A fast hardware-level one (the IDF task watchdog, ~10 s) catches a frozen main loop. A slower app-level one (`WatchdogService`, ~4 minutes) catches devices that are technically alive but stuck off-network or starved of memory.
2. **Visible status in the dashboard.** When a Reader or Writer goes offline the page shows a snackbar immediately and a banner if it stays offline. When it comes back, a green confirmation toast fires.
3. **Discovery between devices.** Every Reader and every Writer announces itself on the local network using mDNS. The Writer's settings page is set up to list nearby Readers in a dropdown so the user doesn't need to type IPs. *(The actual scan-the-network half of this is currently disabled — see Section 4.)*

Everything below is what's already in `main` for the `SerialReaderWriter` branch.

---

## 2. The watchdog (`src/WatchdogService.{h,cpp}`)

### What it does in plain words

> "If this device has been broken for four minutes, restart it. But never restart it more often than once every four minutes — even if it's permanently broken."

That second clause is the anti-death-loop guarantee. A device that boots into a forever-broken network won't melt itself rebooting every few seconds.

### Two timers, one reset rule

| Timer | Value | What it means |
|---|---|---|
| Grace period from boot | 4 minutes | No restart can fire during this window — gives WiFi, mDNS, OTA, etc. time to settle. |
| Unhealthy limit | 4 minutes | Once the grace period is over, a continuous unhealthy stretch this long triggers a restart. |
| Health-check throttle | 30 seconds | Cheaper than every loop tick; resolution is good enough for the 4 min limit. |

The unhealthy-limit timer **resets to "now"** on every healthy check. So a device that flaps in and out for hours will never accumulate enough continuous-unhealthy time to restart. Only sustained breakage triggers the restart.

### The three health checks

A device counts as healthy at the moment of check if **all three** are true:
1. WiFi STA is connected, **or** at least one client is paired with our hotspot.
2. Free heap is at least 30 KB.
3. The main loop is still running. *(Implicit — see "the IDF task watchdog" below; if the loop hangs, that other watchdog kills the chip in 10 s and this one never gets a chance to think about it.)*

The "AP-client = healthy" rule is intentional: when a user is logged into the device's hotspot to fix a misconfigured WiFi password, restarting them out from under their own browser session would be hostile.

### Persisted last-restart reason

Just before `esp_restart()`, a small JSON file is written to flash at `/config/lastRestartReason.json`:

```json
{ "reason": "wifi_unhealthy_4min", "uptime_ms": 245100, "free_heap": 18432, "wifi_status": 1, "ap_clients": 0 }
```

`begin()` reads this file on the next boot, logs it (`[WD] Previous restart cause: ...`), and exposes it via the `/rest/watchdogStatus` endpoint. The dashboard can show "Last restart: stuck WiFi for 4 min" if/when we wire that in.

Reasons currently emitted: `wifi_unhealthy_4min`, `low_heap`, `unknown`. A power-on (POWERON reset) leaves the file empty — the dashboard will simply not show a "last restart" line.

### The IDF task watchdog (the second layer)

`platformio.ini` configures `CONFIG_ESP_TASK_WDT_TIMEOUT_S=10` for `esp32s3`. By default the IDF only watches IDLE tasks — useful for catching a 100 % CPU loop, but not a *frozen* loop. So in `WatchdogService::begin()` we add the current task (Arduino's `loopTask`) to the IDF watchdog, and `WatchdogService::loop()` calls `esp_task_wdt_reset()` at the very top of every iteration.

What this protects against: any blocking call deep in the main loop (a misbehaving mDNS query, a TCP connect that never returns, a hung HTTP client, etc.). If the loop stops calling `esp_task_wdt_reset` for 10 seconds, the chip panics and reboots. The cause of the original `MdnsBrowser` wedge (Section 4) was exactly this kind of hang — and the IDF WDT subscription is now the safety net for any future variant.

### How to verify the watchdog

1. `pio run -e esp32s3` — should build clean.
2. Flash one device (OTA: `pio run -e esp32s3 -t upload` with `upload_port` set to the device IP).
3. `pio device monitor` — every 30 s you should see one line like:
   `[WD] healthy (heap=187456, sta=up, ap_clients=0)`.
4. Force unhealthy: turn off the AP the device is connected to. Watch:
   - For ~4 min: `[WD] unhealthy but in grace period (Xs left)` (only if you forced this immediately on boot).
   - Then: `[WD] unhealthy for Xs (limit 240s)`.
   - Eventually: `[WD] reason=wifi_unhealthy_4min — restarting in 1s` and a ROM boot.
5. After the reboot: `[WD] Previous restart cause: wifi_unhealthy_4min (uptime was 245100 ms, heap was 18432 B)`.
6. `curl http://<ip>/rest/watchdogStatus` should include `last_restart_reason`.

---

## 3. Disconnect notifications

### What the user sees

**On the Reader's "Writers" tab:**
- A Writer goes offline → red snackbar at the bottom of the page: "Writer-1 disconnected."
- It stays offline for ≥ 15 s → a dismissible Alert banner appears at the top of the tab listing each offline Writer.
- It comes back → green snackbar "Writer-1 reconnected."

**On the Writer's "Status" tab:**
- The Source Reader goes offline → red snackbar "Lost connection to Reader at 192.168.2.60."
- It stays offline → dismissible banner at the top of the tab.
- It comes back → green snackbar "Reader reconnected."

The frontend does **not** drive any reconnect — the device's own backoff loops do that. The UI only mirrors the truth.

### Why a snackbar *and* a banner

The snackbar is loud and immediate but auto-disappears. If the user happened to look away when the alert fired, a flickering connection might never tell them anything was wrong. The banner persists until reconnect (or dismissal), so the next time the user looks at the page they still know.

### Files that implement this

- `interface/src/examples/serial/Writers.tsx` — Reader side. Tracks per-Writer transitions across polled snapshots; fires `enqueueSnackbar` and renders one `Alert` per offline-since timestamp.
- `interface/src/examples/serialwriter/WriterStatus.tsx` — Writer side. Tracks `forwarder.connected` flips against a previous-value ref and fires the same UI primitives.

### Out of scope (deliberate)

- Browser push notifications. The user said "dashboard only".
- Persistent log of every connect/disconnect. Just the live UI.

---

## 4. mDNS — announce works, browse is on hold

### Announce (active)

`MdnsService` runs on every device. Each device publishes:

- Service: `_weighsoft._tcp` on port 80.
- TXT records: `mode=reader|writer`, `name=<friendly>`, `id=<short-hash>`.
- Hostname: matches the device's WiFi hostname.

This coexists cleanly with ArduinoOTA's existing `MDNS.begin()` — both responders share one hostname so OTA flashing keeps working.

### Browse (disabled)

`MdnsBrowser` was implemented to scan every 30 s and cache results for 90 s, exposed via `/rest/discovered`. It's **disabled** in `main.cpp` (`mdnsBrowser = nullptr;`) because the underlying Arduino call `MDNS.queryService("weighsoft", "tcp")` hung the main loop on `.60` instead of returning the documented timeout. Symptom: web server still responding (it runs in its own task), but the Arduino loop frozen — scale data stopped flowing, the watchdog couldn't see ahead because it lives in the same loop, and the device only recovered after a power-cycle.

### Why this is safer now

After the wedge, the IDF task watchdog subscription was added (Section 2). If anything else hangs the main loop in this same way, the chip auto-reboots in 10 seconds rather than waiting for a power-cycle. The `MdnsBrowser` itself stays disabled until it's reimplemented non-blockingly — see "Open follow-up" below.

### What the user sees while browse is disabled

- The Writer's "Source Reader" dropdown shows zero discovered Readers. Manual IP entry still works (and is how the system has always worked).
- The Reader's "Discovered nearby" card shows zero discovered Writers. Connected Writers (those that have actually subscribed) still appear in the regular cards below.

### Open follow-up

Switch the browser from blocking `MDNS.queryService` to either:
- ESP-IDF's `mdns_query_async_new` / `mdns_query_async_get_results`, with a small budget per `loop()` tick, or
- A separate FreeRTOS task with its own IDF-watchdog subscription, posting results back to the main task via a queue.

Either approach removes the failure mode that caused the original wedge.

---

## 5. AP fallback (Phase E — verified)

The framework's existing `AP_MODE_DISCONNECTED` behaviour already covers "STA fails → hotspot turns on so the user can reach the device for triage". Phase E was a verification pass:

- A fresh device with no credentials → AP `Weighsoft-Esp-NEW` appears.
- A device with bad WiFi credentials → STA fails, AP comes up after the framework's grace period.
- A user logged into the AP → counts as healthy in the watchdog (Section 2's "AP-client = healthy" rule). The user can finish setup without being kicked off by a restart.

No code changes were needed for this phase.

---

## 6. Quick mental map of the resilience surface

```
                       ┌────────────────────────────────────────┐
                       │ IDF task watchdog (10s) — kills frozen │
                       │ main loop. Subscribed in begin().      │
                       └────────────────────────────────────────┘
                                          ▲
                                          │ esp_task_wdt_reset() every loop()
┌──────────────────────────────────────────────────────────────────────────┐
│ Arduino main loop                                                         │
│  ├─ services run (Serial, KnownWriters, Forwarder, Mdns, …)              │
│  └─ WatchdogService::loop()                                              │
│      ├─ feed IDF WDT                                                     │
│      └─ every 30s:                                                       │
│           healthy?  yes → bump _lastHealthyMs                            │
│                     no  → grace?      yes → log, return                  │
│                           limit hit?  no  → log, return                  │
│                                       yes → persist reason, esp_restart  │
└──────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│ Dashboard (poll every 1–2s)                                              │
│  ├─ Reader Writers tab: per-Writer online/offline → snackbar + banner    │
│  └─ Writer Status tab : forwarder.connected      → snackbar + banner     │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Files that ship Phases A–E

| Path | Phase | Notes |
|---|---|---|
| `src/WatchdogService.h` | A | new |
| `src/WatchdogService.cpp` | A | new |
| `src/MdnsService.h` | C | new (active) |
| `src/MdnsService.cpp` | C | new (active) |
| `src/MdnsBrowser.h` | C | new (inert until non-blocking rewrite) |
| `src/MdnsBrowser.cpp` | C | new (inert until non-blocking rewrite) |
| `src/main.cpp` | A, C | wires services; `mdnsBrowser = nullptr;` |
| `platformio.ini` | A | adds `CONFIG_ESP_TASK_WDT_TIMEOUT_S=10` |
| `interface/src/examples/serial/Writers.tsx` | B, D | snackbar/banner + Discovered nearby |
| `interface/src/examples/serialwriter/WriterStatus.tsx` | B | snackbar/banner |
| `interface/src/examples/serialwriter/WriterSettings.tsx` | D | Source Reader dropdown |
| `interface/src/api/discovered.ts` | D | new |
| `interface/src/types/discovered.ts` | D | new |
