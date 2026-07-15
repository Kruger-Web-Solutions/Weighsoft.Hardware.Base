# Pi USB Reader & Dashboard

Read 4 RS232 silo scales **straight into a Raspberry Pi over USB-serial cables** (no ESP32, no WiFi),
clean up each reading, show it live, store it, and let the customer rename silos from the screen.

Built for **Topline Plastic** (4 silo scales) but reusable for any RS232-scale → Pi → dashboard job.

---

## What's in here

| File | What it is |
|------|-----------|
| `build_silo_system.py` | **The source of truth.** A Python script that builds + deploys the whole Node-RED flow via the Node-RED admin API. Re-run it to redeploy from scratch. |
| `nodered-full-flows-backup.json` | Full export of the Pi's Node-RED at build time (all tabs) — a complete restore point. |
| `PLAN-topline-usb-cutover.md` | The full plan: sockets → dashboard → ESP removal, decisions, risks, phased rollout, status log. |

## What it does (as built)

- Reads 4 USB-serial cables by their **stable physical-socket address** (`/dev/serial/by-path/...`), 9600 8N1, lines split on CR.
- **Robust parser**: handles decimals, thousands separators, sign, `ST`/`US` (stable/unstable), `OL`/overload; converts to kg (float); flags out-of-range vs each silo's capacity.
- **Live dashboard** page "Silos" (`http://<pi>:3000/ui`): per-silo name + weight + ✓/settling/⚠ + gauge; offline alarm (>15 s no data).
- **Editable "Silo Settings" page**: dropdown + name / capacity / material + Save → persisted to `/root/.node-red/silo_settings.json` (auto-created on first run). The stored tag stays `silo_id` so renaming never fragments history.
- **Storage** to InfluxDB (db `silo`, measurement `silo_weight`, tag `silo_id`, fields `weight_kg`/`stable`/`status`) with report-by-exception (20 kg deadband) + 5-min heartbeat.
- **Cable-swap alarm** (safety layer on top of the socket method): every 30 s it reads which cable ID sits in each socket and compares to a learned baseline (`/root/.node-red/silo_cablemap.json`). If a cable is moved to a different socket, the top of the Silos page shows **⚠ CABLE MOVED** (naming the silo + cable); otherwise **✅ all cables in their correct sockets**. An **"Accept current cable layout"** button on the Settings page re-learns the baseline after an intentional rewire. Uses the (stable-but-counterfeit) cable IDs only as a *check* — identity still comes from the socket, so it works even if a cable reports a blank ID.

## Key design decision — identity is by SOCKET, not by cable

The cheap Prolific/ATEN USB-serial adapters used here have **blank/duplicate serial numbers**, so the Pi
can't tell one cable from another. Each silo is therefore tied to its **physical hub port** (`by-path`),
proven stable across power-cycles. **Golden rule: a cable never changes sockets, and the hub never changes
Pi port.** Label every cable + hole + the hub's Pi port. (If you want the reading to follow the *cable*
instead, use genuine **FTDI** cables — each has a real unique ID — and key the flow on that.)

## How to redeploy (bring it back)

On the Pi (Node-RED admin API open on `:3000`):

```bash
# copy build_silo_system.py to the Pi, then:
python3 build_silo_system.py     # rebuilds/replaces the "Silo System" tab (existing flows untouched)
```

Or import `nodered-full-flows-backup.json` via the Node-RED editor (Menu → Import).

**Before relying on serial:** re-derive the `by-path` strings for the actual Pi/hub in use
(`ls -l /dev/serial/by-path/`), and update `BYPATH` / the socket→silo map in the script if the hardware differs.

## Environment (lab, as built 2026-07-15)

- Pi: `esp-dashboard` (Tailscale `100.89.53.60`), Pi 4 2 GB, Raspbian Bookworm (32-bit), Node-RED runs as **root** (userDir `/root/.node-red`), admin API open on `:3000`, InfluxDB **1.8.10** on `:8086`.
- Dashboard: `http://100.89.53.60:3000/ui` (Tailscale) or the Pi's LAN IP `:3000/ui`.

## Still pending (production hardening — see PLAN for detail)

- Disable ModemManager; optional udev friendly names `/dev/silo1..4`.
- Move to a high-endurance SD **or** SSD + **64-bit OS + InfluxDB 2.x + Grafana** (lab currently proves it on 32-bit / InfluxDB 1.8).
- Node-RED editor auth + firewall the ports (admin API currently open).
- Automatic off-device backups; UPS→Pi USB signalling for clean auto-shutdown.
- Trend chart (usage-over-time) — the customer's actual goal — not yet on screen.
