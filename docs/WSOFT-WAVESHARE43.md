# W-Soft weigh screen on Hardware.Base (`wsoft_waveshare43`)

Waveshare ESP32-S3-Touch-LCD-4.3 + DJB-M1X junction box over RS485.
The W-Soft firmware logic comes unchanged from Weighsoft.WSoft `esp/src` @ `cfcd012`.
This branch wraps it in framework services so it has a web page, WebSocket and MQTT like the
other devices here.

> Nothing on this device is legal for trade. That needs NRCS approval and on-site verification.
> Approval stays OFF until Rhyno switches it on, with a USB cable.

## 1. What is where

| Part | Path | Notes |
|---|---|---|
| Pure logic (host-tested) | `lib/wsoft_core/` | `trade_logic.*` (identical), `wsoft_view.*`, `wsoft_settings.*`, `hub_pairing.*` (Hub code check, clock from the Hub) |
| ESP firmware | `lib/wsoft_esp/` | `approval.*`, `fonts.h`, `screen_rle.h` identical; `hub.h` + `hub.cpp` extended for the Hub tab (31 Oct 2026, see section 7); `wsoft_screen.cpp` = old `main.cpp` |
| Services | `src/examples/wsoft/` | `WSoftStatusService` (live), `WSoftSettingsService` (options), `WSoftHubService` (Hub link) |
| Wiring | `src/main.cpp` | under `FT_WSOFT` only |
| Web pages | `interface/src/project/wsoft/` | menu **W-Soft**: Live, Settings (admin), Hub (admin), Calibrate (admin), Information |
| Build | `platformio.ini` | `[env:wsoft_waveshare43]`, `[env:native]` |

Check that the copied files still match the source:

```bash
for f in approval.cpp approval.h fonts.h screen_rle.h; do
  git -C ../weighsoft.wsoft show cfcd012:esp/src/$f | diff -q - lib/wsoft_esp/src/$f; done
for f in trade_logic.cpp trade_logic.h; do
  git -C ../weighsoft.wsoft show cfcd012:esp/src/$f | diff -q - lib/wsoft_core/src/$f; done
# hub.h and hub.cpp no longer match cfcd012 on purpose (Hub tab, 31 Oct 2026).
```

## 2. Bench steps (laptop, not the cloud container)

1. `pio test -e native` — host tests (trade logic, live view, settings). Needs `g++` on PATH.
2. `pio run -e wsoft_waveshare43` — builds on Arduino-ESP32 3.1.1 (bench, 26 Oct 2026:
   RAM 16.6 %, flash 35.0 %; ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, AsyncMqttClient 0.9.0,
   ArduinoJson 6.21.6, WebSockets 2.7.3, ESP32_Display_Panel 1.0.5). The framework needed small
   core-3 fixes (handler casts, WiFi.h / esp_mac.h includes, readIP, mDNS rename), all also
   valid on core 2. The first build downloads toolchains (~45 min). Also build two other envs to prove nothing else broke:
   `pio run -e nodemcu_esp32 -e esp32s3` (`wsoft_esp` is in `lib_ignore` for every env but
   this one).
   **Before you build (bench laptop, measured 31 Oct 2026):** a build is either about 2 min
   or about 30 min, and which one is known before it starts. PlatformIO deletes the env's
   build folder, so every one of about 320 files recompiles, when a `.c/.cpp/.h` file was
   added or removed under `include/`, `src/` or `lib/`, when `platformio.ini` changed, or on
   a new env. Otherwise only the changed files recompile. Run **one build per folder at a
   time**: a second `pio run` in the same folder collides with the first. A build that is
   killed before it ends loses its record of finished files and the next run is nearly
   full again. See `docs/aar/aar-2026-10-31-wsoft-two-hour-build.md`.
3. Plug the board's **UART** USB port (CH343, `COM11` on the bench) and flash:
   `pio run -e wsoft_waveshare43 -t upload`. (The `upload_port` in the env says `COM11`.)
   Flashing is an ASKED gate: only with Rhyno's go.
4. `pio device monitor -b 115200`. Expect `[9/10] W-Soft services loaded OK`, `display ready`,
   then `DJB ... LIVE` lines.
5. Wi-Fi: join the AP `Weighsoft-Esp-NEW` (password in `factory_settings.ini`), open
   `http://192.168.4.1`, log in, set Wi-Fi under **Wi-Fi Connection**. **Required:** change the
   admin password and the AP password. The factory ones are in this public repo.
6. Open **W-Soft → Live**. Check the screen journey on the panel and on the page:

   | Bench action | Panel | Live page |
   |---|---|---|
   | Power on | boot, then the weigh screen | `link: starting`, then `live` |
   | Load on the scale | weight + gauge | weight text and grams |
   | Pull the RS485 cable | `NO SIGNAL`, `----` | `No reply`, then `No signal`; weight `----` (null) |
   | DJB alarm | `DJB ALARM` | `DJB alarm` chip |
   | Approval OFF (default) | `NOT FOR TRADE`, `APPROVAL OFF` | `Not for trade`, Approval OFF |

   If the page stays on `Starting` forever, the panel failed to start (the screen loop stops
   before its first poll). Check the serial log for `display_ready` / panel errors.

7. Hub link: open the box's web page, **W-Soft › Hub**, paste the code from the Hub, click
   **Link to Hub**. The box restarts and shows `HUB ONLINE` (section 7). USB `HUB PAIR` +
   `HUB SAVE` still works as a backup.

## 3. USB commands (UART0, 115200 8N1)

| Command | What it does on this build |
|---|---|
| `APPROVAL STATUS` | Prints ON/OFF, audit counter, trade check, next sequence, queue size |
| `APPROVAL LOG` | Prints the last approval switches |
| `APPROVAL ON <technician> <reason>` | Switches approval on (logged, audit counter +1) |
| `APPROVAL OFF <technician> <reason>` | Switches approval off (logged, audit counter +1) |
| `HUB PAIR <code>` | Checks a Hub pairing code and holds it |
| `HUB SAVE` | Saves the held pairing code to flash and restarts |
| `HUB CLEAR` | Forgets the Hub settings and restarts |
| `HUB WIFI` / `HUB PASS` / `HUB OTA` | **No-op here.** Replies `HUB_ERR not used on this build`. Use the web pages |

Approval has no web, MQTT or BLE write path. That is on purpose.

## 4. Web, WebSocket and MQTT

| Path | Who | What |
|---|---|---|
| `GET /rest/wsoftStatus` | logged-in user | Live view (JSON below) |
| `POST /rest/djb` | admin | One DJB register read or write (below). The screen task runs it on Serial1 |
| WebSocket `/ws/wsoftStatus` | logged-in user | Same JSON, pushed when it changes |
| `GET/POST /rest/wsoftSettings` | admin | Options; a bad topic or name returns 400 and keeps the old values |
| WebSocket `/ws/wsoftSettings` | admin | Same options |
| File `/config/wsoftSettings.json` | – | Saved options (LittleFS) |

Live JSON fields: `ready`, `display_ready`, `link` (`starting` / `no_signal` / `no_reply` /
`alarm` / `live`), `linked`, `weight_g` (whole grams, **null** when not linked or the last poll
failed), `weight_text`, `unit`, `decimals`, `stable`, `alarm`, `approval_on`, `for_trade`,
`trade_check`, `audit_counter`, `last_trade_seq`, `trade_queue`, `hub`, `hub_online`, `polls`.

`POST /rest/djb` body: `{"op":"read","channel":1,"field":"weight"}`,
`{"op":"write","channel":1,"field":"ad","value":0}`, or `{"op":"read","field":"unit"}`.
`field` is `ad`, `weight`, `alarm` (channel 1–16), or `unused`, `unit`, `decimal`,
`division`, `stability_range`, `stability_time`, `zero_track_limit`, `zero_track_time`,
`zero_display`, `sampling`, `filter1`, `filter2`, `address`, `baud`, `byte_order`,
`com1_mode`. Addresses are the 41xxx map. Signed long, byte order 4321. Weight and alarm
are read-only. Alarm reads add `alarm_raw_hex` and `alarm_bits` (high-word bits 0, 1, 13,
14, 15). Sampling and both filters are the raw long. Any call is refused (409
`not_linked`) while the scale is not linked, and every write is refused (409 `approval_on`)
while approval is on: an `ad` write is a calibration (0 zero, CW > 0 load) and the scale
parameters are metrological. Out-of-range writes return 400 `ad_range` / `range`: `ad`
0..1000000, `unit` 0..3, `decimal` 0..4, `address` 1..10 (the link scan range), `baud`
0..2, `byte_order` 0 and `com1_mode` 1 (the poll needs both). Writing `address` or `baud`
drops the link until the scan finds the scale again. After a write the poll re-reads unit
and decimals at once.

Options:

| Option | Default | Rule |
|---|---|---|
| `publish_interval_ms` | 1000 | Clamped to 250..60000 |
| `mqtt_enabled` | true | MQTT itself is off until the broker is set on the MQTT page |
| `mqtt_base_path` | `weighsoft/wsoft/#{unique_id}` | ≤ 96 chars, no spaces or `+`, `#` only as `#{...}` |
| `mqtt_name` | `wsoft-#{unique_id}` | ≤ 48 chars. Home Assistant entity name |
| `mqtt_unique_id` | `wsoft-#{unique_id}` | ≤ 48 chars, no spaces. Home Assistant unique id |

MQTT topic: `<mqtt_base_path>/status`, retained, same JSON as the Live view.

Home Assistant discovery (like the LED example): a retained weight sensor in grams at
`homeassistant/sensor/<mqtt_unique_id>/weight/config`, reading `weight_g` from the status topic.
`weight_g` is null with no valid, fresh weight, so the entity shows unknown. When MQTT is
switched off or the topic changes, the old status and discovery messages are cleared (empty
retained message), so no stale weight stays on the broker.

`weight_g` is still sent while the DJB alarm is set (with `alarm: true`). This matches the panel.
Whether a weight during an alarm should go out on MQTT is open for Rhyno.

## 5. How it runs

- The W-Soft screen loop (Modbus poll, up to ~600 ms, plus panel draw, up to ~1 s) runs in its
  own FreeRTOS task on core 1 (8 KB stack, 5 ms pause per pass). The Arduino loop keeps the web
  server, MQTT and the framework moving.
- After every poll the screen task writes a snapshot under a spinlock. `WSoftStatusService::loop()`
  copies it and publishes only when something changed, at most once per publish interval.
- The framework owns Wi-Fi, NTP and OTA (`-D WSOFT_ON_BASE`). Approval and the Hub queue still
  use NVS (`Preferences`, namespaces `approval` and `hub`) beside the framework's LittleFS.
- RS485 is `Serial1` RX15/TX16 and belongs to W-Soft. The Serial1 services of this branch are
  not created on this board.

## 6. Known gaps

- **The running screen starves Wi-Fi (bench, 31 Oct 2026).** With the RGB panel running, the
  ESP answers ping in 222-304 ms on average (up to 1 s) and serves its web page at about 2.8 KB/s;
  other hosts on the same LAN answer in 1-3 ms. The same firmware with the panel not started
  answers in 5 ms and serves 745 KB/s. Wi-Fi power saving is already off
  (`WiFiSettingsService`), so that is not the cause. Effect: a Wi-Fi firmware update stalls
  (36 KB in 27 s, then "premature end") and the web pages load slowly. A Wi-Fi update that
  arrives while the panel is not running completes. Fix still to do: stop or slow the panel
  during an update (and look at panel clock / bounce buffer / Wi-Fi buffers in internal RAM for
  normal use). Until then flash over USB.
- **After a successful Wi-Fi update the board boots from `app1`.** A plain
  `esptool write_flash 0x10000` then writes `app0`, which the bootloader ignores. Use
  `pio run -e wsoft_waveshare43 -t upload` (it rewrites the boot selection), or erase `otadata`
  first: `esptool.py --chip esp32s3 --port COMx erase_region 0xe000 0x2000`.

- Flashed on the bench board (26 Oct 2026, COM10, no backup of the old firmware). The first
  flash boot-looped: on core 3 `sntp_stop()` asserts without the lwIP core lock, and the
  framework calls it at boot when there is no Wi-Fi. `NTPSettingsService` now takes the lock
  (same pattern as the core's `configTime()`), and `WiFiSettingsService` skips
  `WiFi.mode(WIFI_MODE_MAX)`, which core 3 rejects. After that: 0 reboots in 105 s.
- The board's COM10 port is the ESP32-S3 USB-Serial-JTAG (boot ROM and `ESP_LOG` only). The
  app's `Serial` log goes to UART0, the board's other USB-C port.
- Framework OTA is off by default (`FACTORY_OTA_ENABLED=false`). Jurien chose (31 Oct 2026) to
  allow network flashing: an admin switches it on at **System › OTA Settings** (Enable + Save).
  The password stays the factory one on office prototypes (Jurien, 31 Oct). Before a site
  install, set private OTA and admin passwords: both factory ones are in this public repo. Flash from the laptop:
  `python <platformio>/packages/framework-arduinoespressif32/tools/espota.py -i <ESP IP> -p 8266
  -a <password> -f .pio/build/wsoft_waveshare43/firmware.bin -r`. The screen keeps polling during
  an upload: `hub_ota_busy()` only knows W-Soft's own OTA.
- `default_16MB.csv` is not a project file, so `scripts/clear_otadata.py` skips the otadata
  erase. After an OTA update, a USB flash may boot the older slot; erase flash if that happens.
- The W-Soft menu shows on every board (like the other device menus); on other boards its
  pages get 404. The reverse is also true: on the W-Soft board the Serial, Diagnostics and
  Weight Forwarder menus show, and their pages get 404 (those services own Serial1 and are
  not created here).
- Web firmware upload is off on this board (`FT_UPLOAD_FIRMWARE=0`): flash over USB only, so
  nobody on the AP can load other firmware.
- The app watchdog does not restart this board for lost Wi-Fi (an offline site still weighs);
  it still restarts on low heap.
- The core-2 envs are pinned to `espressif32@6.13.0` (Arduino 2.0.17). Unpinned, a fresh
  PlatformIO install now resolves Arduino-ESP32 3.3.x and `nodemcu_esp32` fails on BLE.
  Bench 26 Oct 2026 on the pin: nodemcu_esp32 RAM 20.0 % / flash 75.9 %, esp32s3 RAM 19.3 % /
  flash 68.1 %.
- No CI job runs `pio test -e native` yet; `verify_build.yml` only runs for PRs into `master`
  (and on GitHub-hosted runners, a pre-existing gap). Run the host tests by hand.

## 7. Link to the Hub from the web page (added 31 Oct 2026)

```
Hub (laptop browser)                       W-Soft web page (admin)
Devices › New pairing code › Copy  ──────▶ W-Soft › Hub › paste › Link to Hub
                                                  │ code checked, saved to NVS "hub/pair"
                                                  ▼ box restarts
                                           HUB CLOCK → (time from the Hub) → HUB ONLINE
```

- **Endpoints** (`src/examples/wsoft/WSoftHubService.*`, admin JWT):
  `GET /rest/wsoftHub` (state, Hub address, key id, last Hub status, clock; never the secret),
  `POST /rest/wsoftHub {"code": "..."}` (400 + message for a bad code; 200 then restart),
  `DELETE /rest/wsoftHub` (forget the code, restart).
- **Same storage as USB.** The web page writes the same NVS key that `HUB PAIR` + `HUB SAVE`
  writes, and the same heartbeat code runs. Approval is not on the web page: it stays USB only.
- **Clock without internet.** The Hub rejects a signature more than 300 s off its own clock.
  When the box has no valid time (no internet NTP, e.g. the bench TOTOLINK), it asks the Hub:
  `GET <hub>/api/v1/health` every 5 s and sets its clock from the reply's `Date` header. NTP
  still wins when it works. The page shows "clock set from the Hub".
- **Hub address.** The code carries the address the Hub page was opened with. The page warns when
  it is a Tailscale `100.64–127.x` address or `localhost`: the box can't reach those. Open the Hub
  with the laptop's LAN address (e.g. `http://192.168.1.x:8095`) before making the code.
- **Host tests:** `test/test_hub_pairing` (real Hub-format codes made like
  `Weighsoft.Indicator.Hub server/src/registry/registry.ts`, test secret only).

## 8. Calibrate tab (added 31 Oct 2026)

**W-Soft › Calibrate** (admin) walks through Zero → Span → Check using `POST /rest/djb`
(`field: "ad"`, channel 1). Zero sends `0`. Span sends the test weight as display digits
(weight × 10^decimals, e.g. 2.000 kg with 3 decimals = `2000`), per the Supmeter manual's
"Calibrating Weight" range. Each step asks to confirm: the old calibration is overwritten.
Refused while approval is on or the DJB is not linked. **Not yet bench-confirmed:** check the
first span against the reading; stop if it is 10× or 1000× off.

## 9. Saving weighings (added 31 Oct 2026)

```
load on ──► still for N s ──► AUTO-SAVE (once) ──► take the load off (back to 0) ──► ready again
                 │
                 └─► or tap SAVE (screen) / Save (W-Soft › Live) once it is still for 1 s
```

- **Settings** (W-Soft › Settings › Saving weighings, admin): *Auto-save* on/off (default off)
  and *Stable for N seconds* (default 3, clamped 1–30). Stored in `/config/wsoftSettings.json`
  as `auto_save` and `auto_save_stable_s`.
- **Rules** (`lib/wsoft_core/src/save_logic.*`, host tests `test/test_save_logic`): stable =
  fresh, unchanged value/decimals/unit (same as the trade rule); a failed poll restarts the
  clock. Manual SAVE needs 1 s still (the operator is watching); auto-save waits N s. One save
  per load: the next one only after a fresh reading ≤ 0. Blocked reasons: `moving`, `alarm`,
  `no_signal`, `stale`, `empty`, `saved` (this load is done), `unit` (no kg meaning or more
  than 5 kg decimals), `no_clock`, `queue_full`.
- **Screen.** SAVE button in the right of the ZONE box (GT911 touch from the ESP32_Display_Panel
  board config; started like `Board::begin()` does it). Green = a tap saves; grey = not now.
  The line under the approval line shows `READY`, `MOVING`, `EMPTY`, `SAVED n`, `QUEUE FULL`
  ... and `AUTO nS` when auto-save is on. Serial log: `SAVE seq=... weight=... auto|button`.
  If touch does not start, the log says so and the web Save still works.
- **Web** (`src/examples/wsoft/WSoftStatusService.*`, any logged-in user):
  `POST /rest/wsoftSave` (200 `{ok,seq,weight_text}`, 409 `{ok:false,reason}`, 503 `busy`),
  `GET /rest/wsoftSave` (last 10 since start-up plus what still waits, newest first, sent or
  waiting). The status WebSocket/MQTT adds `save_can`, `save_reason`, `save_auto`,
  `save_auto_s`, `save_last_seq`, `save_last_text`, `save_queue`, `save_hub_status`.
- **Flash queue** (`lib/wsoft_esp/src/save_queue.*`, NVS namespace `wsave`): 32 weighings,
  written before they count, so a power cut keeps them. Full = SAVE refused (`QUEUE FULL`),
  never a drop.
- **To the Hub** (`hub.cpp`): oldest first, signed `POST /api/v1/ingest/weighings` (same
  signing as the heartbeat), one a second. Body: `weighing_id` `wsoft-<mac>-<series>-<seq>`
  (series is random per flash life, so a wiped box never reuses an id), `committed_at` UTC,
  `gross_kg` = `net_kg` = the exact reading in decimal kg (Hub PR #78: up to 5.5 digits, never
  rounded), `tare_kg` 0, `basis` `GS`, `payload` `{source:"wsoft", seq, value, decimals, unit,
  trade, auto_saved}`. 201 or 200 (duplicate) = delivered. 409/429/5xx/no network: retry after
  5 s, doubling to 60 s. Other 4xx (e.g. 400, 401): kept, logged, tried again every 60 s.
  Nothing is sent during a firmware upload. No Hub paired = they wait in the queue.
- **Approval.** Unchanged. With approval ON the trade weight still goes out in the heartbeat as
  before; a saved weighing is extra and carries `trade: true`. With approval OFF it carries
  `trade: false` (not for trade).
- **Not yet bench-confirmed:** the touch button (GT911 coordinates on this panel), the auto-save
  feel at 3 s, and both weighings showing on the test Hub.
