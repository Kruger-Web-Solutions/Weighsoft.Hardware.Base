# Task log: Mandatory roadmap / decision-log workflow

**Purpose:** Checklist so firmware investigations stay traceable and do not mix unrelated root causes (e.g. STA Wi‑Fi vs outbound WebSocket TX).

## When changing runtime behavior

1. **Identify subsystem** from logs (heap, `WiFi.isConnected`, `msSinceWfLoop`, AsyncWebSocket vs `WebSocketsClient`, UART availability).
2. **Record evidence** in the appropriate draft decision log under `docs/drafts/` (for this track: [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.5 attempt table + §9.3 symptom rows).
3. **Cross-link** if symptoms could be mistaken for another area (example: [DECISION-LOG-wifi-sta-connectivity.md](../drafts/DECISION-LOG-wifi-sta-connectivity.md) for “errno 11 + ws_tx_FAIL” vs STA association).
4. **Update `docs/task-logs/`** for the active product task so operators know what was fixed and how to re-verify (see [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md)).
5. **Build smoke:** add a row to §9.6 (or equivalent) when running `pio run` / upload for that attempt.

## 2026-04-24 update — hardware target + task log

- **Serial Reader on ESP32-S3 (COM2):** The active firmware image is still the **Weighsoft Serial** app (`src/main.cpp` — serial ingress + forwarder + local Async WebSocket). For USB flashing to a specific bench port, `[env:esp32s3]` in `platformio.ini` now uses **`upload_port = COM2`** (was `COM11`) with **`upload_protocol = esptool`**. This is a **port binding** for upload only; it does not change runtime behavior. Operators on other USB ports should override locally or temporarily set `upload_port` / use `pio run -e esp32s3 -t upload --upload-port <PORT>`.
- **Task log cross-link:** [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) §2026-04-24 documents the same change and re-verification steps (operator section remains source of truth for post-flash checks).
- **Build smoke (2026-04-24):** `pio run -e esp32s3 -t upload` — **compile/link OK** for `esp32s3`; **upload not completed** in agent session (`COM2` missing / not accessible: `FileNotFoundError`). Does not block decision-log content; append a successful on-bench upload row to [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.6 when an operator confirms flash on hardware.

## 2026-04-24 update — upload vs firmware (classification)

- **Separate failure classes:** **USB serial / `esptool` upload** (ROM bootloader, COM port, drivers, cable, BOOT/RESET) is **not** the same subsystem as **STA Wi‑Fi**, **outbound `WebSocketsClient`**, or **AsyncWebSocket** queue pressure documented in §2026-04-22. Do not merge upload troubleshooting into the serial-bridge runtime attempt table without labeling it **tooling/host**.
- **In-repo hardware spec for `esp32s3`:** See [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) section **“ESP32-S3 upload failures: hardware & driver compatibility”** — summarizes `board = esp32-s3-devkitc-1`, **`partitions_ble_ota.csv` requires ≥8 MB flash**, `esptool` expectations, **`ARDUINO_USB_*` flags** (runtime serial only), Windows **VID** → driver mapping, dual-USB jack note, and **`flash_id` / `pio device list`** commands for evidence.
- **Agent limitation:** Driver correctness for a specific PC cannot be verified from the repository; only **Device Manager Hardware Ids** + successful **`esptool … flash_id`** on the bench close that loop.

## 2026-04-24 update — vendored Windows USB drivers

- **Location:** [`docs/vendor-drivers/windows/`](../vendor-drivers/windows/) — contains **Espressif** `ESP32.USB.JTAG-v6.1.7600.16386.zip` and **Silicon Labs** `CP210x_Universal_Windows_Driver.zip`, plus [`README.md`](../vendor-drivers/windows/README.md) and [`SOURCES.txt`](../vendor-drivers/windows/SOURCES.txt) (URLs + install summary).
- **Rationale:** Supports **offline** driver install for the most common ESP32-S3 bench cases (`VID_303A` native USB path, **CP210x** UART bridge). **WCH CH343/CH340** is documented in README with an external link only (not mirrored in-repo in this pass).
- **Cross-link:** [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) subsection **“Windows drivers vendored in-repo”** ties this to the upload-failure investigation.

## 2026-04-24 update — ESP32-S3 STA + captive soft AP (Wi‑Fi stack)

- **Symptom class:** ESP32-S3 “no internet” / unreliable setup access — treat as **STA + soft AP + radio coexistence**, not serial-forwarder or outbound WebSocket issues until logs prove otherwise.
- **`[env:esp32s3]` (`platformio.ini`):** Added **`-D FT_BLE=0`** so this board profile does not compile BLE (see `features.ini` default `FT_BLE=1` for other ESP32 envs). **Rationale:** BLE and Wi‑Fi share 2.4 GHz; starting `BLEDevice::init` while STA is joining often degrades or blocks association on ESP32-class boards. Re-enable BLE for S3 only if you add a coexistence strategy and bench validation.
- **LittleFS defaults (`data/config/`):** New **`apSettings.json`** with **`provision_mode`: 0** (`AP_MODE_ALWAYS`) so the **soft AP + captive DNS** stay up even when STA is connected — operators can always reach **`192.168.4.1`** for Wi‑Fi setup. **`bleSettings.json`** default **`enabled`: false** so filesystem-first boots avoid advertising BLE when the firmware build includes BLE on other targets.
- **Framework (`lib/framework/`):** `APSettingsService::startAP()` on **ESP32** now sets **`WIFI_AP_STA`** when the chip is already in **STA** mode before `softAP`, matching Espressif expectations for concurrent AP + STA. `WiFiSettingsService::manageSTA()` calls **`WiFi.setSleep(false)`** before **`WiFi.begin`** on ESP32 to reduce modem-sleep-related disconnects. **`APSettings::update`** fixed the **`provision_mode`** validation switch to use **`newSettings.provisionMode`** (was incorrectly reading stale **`settings`**).
- **Cross-link:** Operational checks remain in [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md); append STA verification notes there when bench evidence exists.
- **LittleFS:** New defaults under `data/config/` require **`pio run -e esp32s3 -t uploadfs`** (or factory reset + image that includes the updated FS) on devices that already had a populated **`/config/`** partition; firmware-only `upload` does not always refresh LittleFS.

## 2026-04-22 update

- Added **§9.5 #15** to serial-scale decision log: defer `forwardWeight` out of `SerialService::update` handler chain; documented COM10 log pattern (`serialMs` ~18415 with `msSinceWfLoop` ~8403 at `sendTXT_false`).
- Extended Wi‑Fi draft log changelog + evidence trail to **#15** so readers do not attribute outbound client starvation to STA credential bugs.
