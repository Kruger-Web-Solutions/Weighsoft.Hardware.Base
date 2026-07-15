# Topline Plastic — Move 4 Silo Scales from ESP/WiFi to Pi USB Cables

**Date:** 2026-07-15 · **Site:** Topline Plastic (South Africa) · **Pi:** `esp-dashboard` (Tailscale 100.89.53.60)
**Goal:** the dashboard reads the 4 silo scales **directly from the Raspberry Pi over USB-serial cables**; the 4 ESP32 boxes and all WiFi are **removed**.
**Status:** Design complete and critique-hardened. **Not yet safe to execute** until the three gating fixes below are done — then execute phase by phase.

> This plan supersedes the WiFi/ESP design for Topline. It was built from a live hardware investigation (sockets, cables, scale data all confirmed on the real Pi) and reviewed by an adversarial completeness critic.

---

## 0. Decisions — CONFIRMED with the Product Owner (2026-07-15)

| # | Decision | Answer |
|---|----------|--------|
| 1 | Silo names | Default **Silo 1–4**, **editable from the dashboard** |
| 2 | Capacity (fault bound) | Default **70,000 kg** per silo, **editable from the dashboard** |
| 3 | Material per silo | **Unknown yet** — blank, **editable from the dashboard** |
| 4 | UPS (load-shedding) | **Customer already has one** — confirm both Pi + hub are on it; wire USB signalling for clean auto-shutdown if the UPS supports it |
| 5 | On-site hands | **A Topline technician** (name TBD) — deliver a printable on-site runbook addressed to them |

### Firm requirement: customer-editable "Silo Settings" page
Name, capacity, and material must be editable by the customer from the dashboard (no config files, no technician). Design: the **stored tag stays the stable `silo_id` (silo1..4)** so history never fragments on rename; the editable name/capacity/material live in a small persisted settings store (Node-RED file/context) that the parser reads at runtime (capacity → plausibility bound) and the dashboard reads for display. One editable page covers all three fields for all four silos.

---

## Site survey of the Pi (2026-07-15, read-only)

| Finding | Detail | Consequence |
|---------|--------|-------------|
| **Pi 4 Model B, 1 GB RAM** | only ~400 MB available | 1 GB is tight for InfluxDB 2 + Grafana + Node-RED 24/7 — decide: bigger Pi vs lean build (Node-RED dashboard, no Grafana) |
| **32-bit userland on 64-bit kernel** | `getconf LONG_BIT`=32, arch aarch64, Raspbian Bookworm armhf | InfluxDB 2.x needs 64-bit → **clean 64-bit OS install on the SSD required** (cannot clone the SD) |
| **Root on SD card, no SSD yet** | `/dev/mmcblk0p2`, 58 GB, 13% used | Fit the USB SSD on-site, clean 64-bit install |
| **InfluxDB service inactive; Node-RED v4.1.8 active; Grafana inactive** | current flow points at 1.8 but service is off | Combined with the placeholder-config bug, the current system is **very likely storing no data** — little/nothing to migrate |
| **ModemManager ACTIVE** | grabs new ttyUSB ports and injects AT commands | **Must disable** (`sudo systemctl disable --now ModemManager`) before using the serial cables — prerequisite |
| **pi in dialout; 4 serial-by-path present** | serial permissions OK | good |
| **Clock/timezone/NTP all good** | Africa/Johannesburg, synchronized, NTP active | no RTC needed as long as the LAN reaches a time source |
| **UPS not linked to the Pi by USB** | no HID UPS on bus, no apcupsd/nut installed | UPS provides ride-through power only; for clean auto-shutdown the tech must connect the UPS USB cable + install NUT/apcupsd (if the UPS model supports signalling) |
| **Tailscale up (100.89.53.60), SSH key auth** | remote support link works | good |

**RESOLVED (PO decision 2026-07-15):** upgrade to a **bigger Pi (4 GB+)** — Pi 4 (4 GB) or Pi 5 both fine (Pi 5 can boot NVMe, even faster). Full Grafana stack is viable. The current 1 GB Pi becomes the on-site spare.
**Note:** a new Pi has a **different USB topology**, so the exact `by-path`/`KERNELS` socket strings we confirmed on this Pi 4 must be **re-derived on the new hardware** (same method — identify by physical socket — just new path strings). Keep the same hub in the same new-Pi port for life.

## The three gating fixes (from the critique — do these before cutover)

1. **Reconcile the config method.** Use **one** naming method only: udev friendly names `/dev/silo1..4` via a **single** rules file `/etc/udev/rules.d/99-topline-silos.rules`, applied with `sudo udevadm control --reload && sudo udevadm trigger`. Disable **ModemManager** (`sudo systemctl disable --now ModemManager`) so it can't grab the serial ports. Create the InfluxDB 2.x bucket + token **before** wiring the writer.
2. **Add a power story for load-shedding.** UPS + configured safe shutdown; USB-SSD boot; a **mains-powered** USB hub on the same UPS; auto-start InfluxDB/Node-RED/Grafana on boot; NTP clock check (the Pi has no battery clock, so after a power cut it boots with the wrong time until it syncs).
3. **Add a commissioning acceptance gate before removing any ESP.** Capture ~2 minutes of raw output from each **real** silo indicator, confirm the baud/format, and **cross-check each socket's weight against the indicator's own display** (this is the one test that catches a cable in the wrong socket). Run the Pi path **in parallel** with the ESPs, compare, and pull the ESPs only after it passes.

---

## Silo ⇄ socket configuration (the heart of it)

Identity comes from the **physical hub socket**, never the cable's serial number — the adapters are counterfeit Prolific/ATEN with blank/duplicate serials (observed: blank, "D", three sharing "b153609"). The socket path is **proven stable across a full power-cycle**.

**Golden rule (write it on the hub): a cable never changes sockets, and the hub never changes Pi port.**

| Socket | Kernel port | Friendly name | by-path device (the stable address) | Silo | Status (last check) |
|--------|-------------|---------------|--------------------------------------|------|---------------------|
| 1 | `1-1.1` | `/dev/silo1` | `/dev/serial/by-path/platform-fd500000.pcie-pci-0000:01:00.0-usb-0:1.1:1.0-port0` | SILO_A | scale +300 kg |
| 2 | `1-1.2` | `/dev/silo2` | `…-usb-0:1.2:1.0-port0` | SILO_B | scale +105 kg |
| 3 | `1-1.3` | `/dev/silo3` | `…-usb-0:1.3:1.0-port0` | SILO_C | **SPARE — no scale yet (stays silent until fitted)** |
| 4 | `1-1.4` | `/dev/silo4` | `…-usb-0:1.4:1.0-port0` | SILO_D | scale +300 kg |

**udev rules** — `/etc/udev/rules.d/99-topline-silos.rules`:
```
SUBSYSTEM=="tty", SUBSYSTEMS=="usb", KERNELS=="1-1.1", SYMLINK+="silo1", GROUP="dialout", MODE="0660"
SUBSYSTEM=="tty", SUBSYSTEMS=="usb", KERNELS=="1-1.2", SYMLINK+="silo2", GROUP="dialout", MODE="0660"
SUBSYSTEM=="tty", SUBSYSTEMS=="usb", KERNELS=="1-1.3", SYMLINK+="silo3", GROUP="dialout", MODE="0660"
SUBSYSTEM=="tty", SUBSYSTEMS=="usb", KERNELS=="1-1.4", SYMLINK+="silo4", GROUP="dialout", MODE="0660"
```
Apply: `sudo udevadm control --reload && sudo udevadm trigger` · Verify: `ls -l /dev/silo*`
Record the mapping in `/etc/topline/silomap` (single source of truth) and keep a copy in git.

**Label three things:** each cable at both ends (`SOCKET 1`…`4`), each hub hole (`1`…`4`), and the Pi port the hub sits in (`HUB — DO NOT MOVE`). Physically fix the hub and strain-relieve every cable so a tug can't pull the hub out (that would shift every socket path).

**Replace a dead cable:** new adapter into the **same** hole → done, no config change. **Add silo 3's scale:** plug it into the socket-3 adapter, flip its status to active in `silomap`, enable its Node-RED input.

---

## The phases

### Phase 0 — Prep (offline, zero disruption; ESPs keep running)
- Confirm the Pi is **64-bit** (`getconf LONG_BIT` → 64); if 32-bit, do a clean 64-bit OS install on the SSD.
- Boot the Pi from a **USB SSD** (SD cards corrupt within months of continuous writes). Set USB boot order; use a UASP-capable USB3 enclosure in a blue port.
- Fit a **UPS**; configure safe shutdown on low battery. Put the **mains-powered USB hub** on the same UPS.
- Confirm **NTP** time sync (store UTC, display Africa/Johannesburg SAST). Disable **ModemManager**.
- Install **InfluxDB 2.7 OSS**; create org `topline`, buckets + retention (below), and an API token.
- Harden: put a password on the Node-RED editor, admin passwords on Grafana/Influx, and a firewall (`ufw`) limiting ports 1880/3000/8086 to localhost + the tailnet only.
- Build the **new Node-RED flow** and test it against the already-connected live cables.

### Phase 1 — Lock the sockets & names
- Apply the udev rules, create `/etc/topline/silomap`, label cables/sockets/hub port, fix the hub in place.

### Phase 2 — Rebuild the dashboard software
- Replace the WiFi listener with **4 serial-in nodes** (`/dev/silo1..4`, 9600 8N1, split on `\r`).
- **Robust parser** (replaces the integer-only one that mis-read decimals and thousands): handles decimals/sign/space-grouping, ST/US stability, OL/UL/-----/E fault tokens, unit→kg, stores **float**.
- **Validation:** silo whitelist + plausibility bounds (0…capacity).
- **Deadband + heartbeat:** only write on real change + one heartbeat/min (keeps the DB small).
- **Offline + frozen watchdog:** offline = no raw bytes for >15 s; frozen = bytes arriving but value byte-identical for >2 min. Edge-triggered alerts. (Frozen keys off **raw serial silence**, not the derived weight.)
- **InfluxDB write fixed:** real bucket + token (the old config had placeholders and was saving nothing). Schema: measurement `silo_weight`, tag `silo_id`, fields `weight_kg` (float), `status`, `stable`, `raw`.
- **Catch node** + local buffer file so a brief DB outage doesn't lose readings.

### Phase 3 — Commissioning acceptance gate (the safety net — do NOT skip)
- Capture ~2 min raw from each **real** silo indicator; confirm baud/format; tune the parser.
- **Cross-check every `/dev/siloN` weight against that silo's own displayed weight** — proves the right scale is on the right socket.
- Run the Pi path **in parallel** with the ESPs (optional Y-split of the transmit line) and compare.

### Phase 4 — Cut over one silo at a time
- Move each scale's serial from its ESP to the matching Pi socket (hard cutover), verify, then the next.

### Phase 5 — Decommission the ESPs & retire the old flow
- Only after Phase 3 passes and parallel-run agrees: export/backup the old Node-RED flow, disable it, power off and store the 4 ESPs as spares. No firmware work needed.

### Phase 6 — Operations & handover
- Storage: buckets `weight_raw` (90 d), `weight_1m` (2 y), `weight_1h` (∞) with downsampling tasks.
- Dashboards: **Grafana** for live weight + usage trends (usage = sum of negative deltas; refills excluded), **Node-RED** status screen for on-site "all 4 silos reporting".
- Backups **off the Pi**: nightly Influx backup to a second USB stick + off-site copy over Tailscale; image the commissioned SSD; keep a **spare Pi** so recovery is a swap.
- **Dead-man alert** (out-of-band) so a crashed Node-RED / dead Pi is noticed, not discovered days later.
- One-page **plain-language runbook** for the on-site person + a who-does-what split (on-site physical vs. remote over Tailscale).

---

## New hardware to buy (rough)

| Item | Why | Approx |
|------|-----|--------|
| **Raspberry Pi 4 (4 GB) or Pi 5** | 1 GB is too tight for DB + Grafana 24/7 (PO chose upgrade) | ~R1,000–1,600 |
| USB SSD (+ UASP enclosure; or NVMe if Pi 5) | Replace SD card; survives continuous writes | ~R700 |
| Mains-powered 4-port USB hub | Bus-powered hub browns out & drops a silo | ~R400 |
| 2nd USB stick | Nightly backups | ~R150 |
| UPS→Pi USB signalling cable (if UPS supports it) | Clean auto-shutdown on low battery | ~R0–150 |

Already owned: **UPS** (customer has one). The old **1 GB Pi becomes the on-site spare** (swap-to-recover). The scale cables plug straight into the Pi — **no switch, no network island, no WiFi**.

---

## Top risks (all mitigated in the plan)
- **Load-shedding corrupts the DB** → UPS + safe shutdown + SSD + auto-start.
- **Cable in the wrong socket = silently wrong data** → labels + golden rule + commissioning cross-check.
- **Hub moved to another Pi port** → all socket paths shift → fix + label the hub port.
- **Bus-powered hub brownout drops a silo** → mains-powered hub on the UPS.
- **Counterfeit adapter dies/drops** → offline alert + spare adapters.
- **Frozen scale looks alive** → stale detection on raw byte silence, not derived weight.

## Rollback
Keep the ESPs + disabled old flow for ~1–2 weeks. Per-silo rollback = replug that scale into its ESP and re-enable the old flow. One silo at a time means any single silo reverts in minutes without affecting the others.

---

## Status log

- **2026-07-15 — Stage A (lab proof) DONE & VERIFIED.** On the lab Pi (Pi 4 2 GB, still 32-bit/SD): installed `node-red-node-serialport`; built a new Node-RED tab "Silo Live (test)" reading all 4 cables by their stable `by-path` addresses (9600 8N1, split on CR) → resilient parser (decimals/thousands/sign/ST-US/OL, →kg float, 70 000 kg bound) → live `ui_text` dashboard page "Silos" at `:3000/ui`. Verified end-to-end: silo1≈109 kg, silo2≈280 kg, silo4≈280 kg all live; silo3 correctly silent (spare). Existing flows untouched (added on a new tab). Ports were free; the 5 pre-existing serial configs are dormant leftovers.
- **Deferred (need the pi user's sudo — password-protected, so hand to the on-site tech / a one-liner):** `sudo systemctl disable --now ModemManager`; install `/etc/udev/rules.d/99-topline-silos.rules` for friendly `/dev/silo1..4` names (flow currently uses by-path directly, which works). Neither blocks the demo.
- **Environment notes discovered:** Node-RED admin API open (no auth) on **:3000**; InfluxDB **1.8.10** running on :8086 (current dashboard's DB target had placeholder config = not storing); other flows present (Modbus Poller, Weight per Hour, ESP test). Full production stack (SSD + 64-bit + InfluxDB 2 + Grafana + editable Silo Settings page + retention + alerts) = Stage B, still pending the SSD + 64-bit reinstall.

- **2026-07-15 — Full lab system BUILT & VERIFIED (remote, no sudo).** On the lab Pi (now a Pi 4 **2 GB**, still 32-bit/SD; Node-RED runs as **root**, admin API open on **:3000**, InfluxDB **1.8.10** on :8086). Deployed a single "Silo System" Node-RED tab (existing flows untouched; full backup saved to `docs/superpowers/topline/nodered-full-flows-backup.json`, builder saved to `.../build_silo_system.py`):
  - **Read** 4 cables by stable `by-path` (9600 8N1, split CR) → **robust parser** (decimals/thousands/sign, ST/US, OL/overload, →kg float, capacity bound, out-of-range flag).
  - **Live dashboard** page "Silos" (`:3000/ui`): per-silo text readout (custom name + weight + ✓/settling/⚠) + gauge; silo3 shown as spare.
  - **Editable "Silo Settings" page**: dropdown + name/capacity/material inputs + Save → persisted to `/root/.node-red/silo_settings.json` (auto-seeded via error-catch on first run; **persistence verified** — file read back on restart). Stored tag stays `silo_id`; names/capacity are display/validation overlays (rename never fragments history). Parser reads capacity from settings live.
  - **Storage** to InfluxDB 1.8 db `silo`, measurement `silo_weight`, tag `silo_id`, fields `weight_kg`(float)/`stable`/`status`; **report-by-exception** (20 kg deadband) + 5-min heartbeat. Verified rows accumulating, correctly per-silo tagged.
  - **Offline/frozen watchdog** (10 s): marks a silo OFFLINE (>15 s no data) on the live page; silo3 labelled spare.
  - Clean: 0 recent errors. Live: silo1≈112, silo2≈260, silo4≈260 kg.
- **STILL "the rest" (needs the pi/root password or hardware — for the on-site login):** disable ModemManager; udev friendly `/dev/silo1..4` names; **SSD + clean 64-bit OS + InfluxDB 2.x** (production storage/retention/Grafana) — the lab currently proves the concept on 32-bit/SD/Influx 1.8; UPS→Pi USB signalling; Node-RED editor auth + firewall (admin API is currently open on :3000). Deadband/heartbeat/capacity are tunable in the flow.
