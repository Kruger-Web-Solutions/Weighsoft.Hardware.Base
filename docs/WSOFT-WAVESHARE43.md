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
| Pure logic (host-tested) | `lib/wsoft_core/` | `trade_logic.*` (identical), `wsoft_view.*`, `wsoft_settings.*` |
| ESP firmware | `lib/wsoft_esp/` | `approval.*`, `hub.h`, `fonts.h`, `screen_rle.h` identical; `hub.cpp` guarded; `wsoft_screen.cpp` = old `main.cpp` |
| Services | `src/examples/wsoft/` | `WSoftStatusService` (live), `WSoftSettingsService` (options) |
| Wiring | `src/main.cpp` | under `FT_WSOFT` only |
| Web pages | `interface/src/project/wsoft/` | menu **W-Soft**: Live, Settings (admin), Information |
| Build | `platformio.ini` | `[env:wsoft_waveshare43]`, `[env:native]` |

Check that the copied files still match the source:

```bash
for f in approval.cpp approval.h hub.h fonts.h screen_rle.h; do
  git -C ../weighsoft.wsoft show cfcd012:esp/src/$f | diff -q - lib/wsoft_esp/src/$f; done
for f in trade_logic.cpp trade_logic.h; do
  git -C ../weighsoft.wsoft show cfcd012:esp/src/$f | diff -q - lib/wsoft_core/src/$f; done
unifdef -UWSOFT_ON_BASE lib/wsoft_esp/src/hub.cpp | diff -q - <(git -C ../weighsoft.wsoft show cfcd012:esp/src/hub.cpp)
```

## 2. Bench steps (laptop, not the cloud container)

1. `pio test -e native` — host tests (trade logic, live view, settings). Needs `g++` on PATH.
2. `pio run -e wsoft_waveshare43` — **this is the core-3 spike.** The framework has only been
   built on Arduino-ESP32 2.0.x. If it fails, record the error; do not pin library versions
   by guessing. When it builds, pin `ESP32Async/AsyncTCP` and `ESP32Async/ESPAsyncWebServer`
   to the versions that worked.
3. Plug the board's **UART** USB port (CH343, `COM11` on the bench) and flash:
   `pio run -e wsoft_waveshare43 -t upload`. (The `upload_port` in the env says `COM11`.)
   Flashing is an ASKED gate: only with Rhyno's go.
4. `pio device monitor -b 115200`. Expect `[9/10] W-Soft services loaded OK`, `display ready`,
   then `DJB ... LIVE` lines.
5. Wi-Fi: join the AP `Weighsoft-Esp-NEW` (password in `factory_settings.ini`), open
   `http://192.168.4.1`, log in, set Wi-Fi under **Wi-Fi Connection**. Change the admin password.
6. Open **W-Soft → Live**. Check the screen journey on the panel and on the page:

   | Bench action | Panel | Live page |
   |---|---|---|
   | Power on | boot, then the weigh screen | `link: starting`, then `live` |
   | Load on the scale | weight + gauge | weight text and grams |
   | Pull the RS485 cable | `NO SIGNAL`, `----` | `No reply`, then `No signal`; weight `----` (null) |
   | DJB alarm | `DJB ALARM` | `DJB alarm` chip |
   | Approval OFF (default) | `NOT FOR TRADE`, `APPROVAL OFF` | `Not for trade`, Approval OFF |

7. Hub pairing (if needed): over USB send `HUB PAIR <code>` then `HUB SAVE`.

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
| WebSocket `/ws/wsoftStatus` | logged-in user | Same JSON, pushed when it changes |
| `GET/POST /rest/wsoftSettings` | admin | Options; a bad topic or name returns 400 and keeps the old values |
| WebSocket `/ws/wsoftSettings` | admin | Same options |
| File `/config/wsoftSettings.json` | – | Saved options (LittleFS) |

Live JSON fields: `ready`, `display_ready`, `link` (`starting` / `no_signal` / `no_reply` /
`alarm` / `live`), `linked`, `weight_g` (whole grams, **null** when not linked or the last poll
failed), `weight_text`, `unit`, `decimals`, `stable`, `alarm`, `approval_on`, `for_trade`,
`trade_check`, `audit_counter`, `last_trade_seq`, `trade_queue`, `hub`, `hub_online`, `polls`.

Options:

| Option | Default | Rule |
|---|---|---|
| `publish_interval_ms` | 1000 | Clamped to 250..60000 |
| `mqtt_enabled` | true | MQTT itself is off until the broker is set on the MQTT page |
| `mqtt_base_path` | `weighsoft/wsoft/#{unique_id}` | ≤ 96 chars, no spaces or `+`, `#` only as `#{...}` |
| `mqtt_name` | `wsoft-#{unique_id}` | ≤ 48 chars |
| `mqtt_unique_id` | `wsoft-#{unique_id}` | ≤ 48 chars, no spaces |

MQTT topic: `<mqtt_base_path>/status`, retained, same JSON as the Live view.

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

- Not built yet: the core-3 spike (step 2) is the first real test of this env.
- Framework OTA is off (`FACTORY_OTA_ENABLED=false`). If it is switched on later, the screen
  keeps polling during an upload: `hub_ota_busy()` only knows W-Soft's own OTA.
- `default_16MB.csv` is not a project file, so `scripts/clear_otadata.py` skips the otadata
  erase. After an OTA update, a USB flash may boot the older slot; erase flash if that happens.
- The W-Soft menu shows on every board (like the other device menus); on other boards its
  pages get 404.
