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
| Pure logic (host-tested) | `lib/wsoft_core/` | `trade_logic.*` (identical), `wsoft_view.*`, `wsoft_settings.*`, `wsoft_switch.*`, `wsoft_pin.*`, `wsoft_sha256.*`, `wsoft_scale_setup.*`, `wsoft_hub_commands.*` |
| ESP firmware | `lib/wsoft_esp/` | `approval.h`, `hub.h`, `fonts.h`, `screen_rle.h` identical; `approval.cpp` and `hub.cpp` guarded; `wsoft_screen.cpp` = old `main.cpp`; new: `switches.*`, `pin.*`, `scale.*`, `approval_base.h` |
| Services | `src/examples/wsoft/` | `WSoftStatusService` (live), `WSoftSettingsService` (options), `WSoftSwitchesService` (switches, PIN, scale setup) |
| Wiring | `src/main.cpp` | under `FT_WSOFT` only |
| Web pages | `interface/src/project/wsoft/` | menu **W-Soft**: Live, Settings (admin), Information |
| Build | `platformio.ini` | `[env:wsoft_waveshare43]`, `[env:native]` |

Check that the copied files still match the source:

```bash
for f in approval.h hub.h fonts.h screen_rle.h; do
  git -C ../weighsoft.wsoft show cfcd012:esp/src/$f | diff -q - lib/wsoft_esp/src/$f; done
for f in trade_logic.cpp trade_logic.h; do
  git -C ../weighsoft.wsoft show cfcd012:esp/src/$f | diff -q - lib/wsoft_core/src/$f; done
for f in approval.cpp hub.cpp; do
  unifdef -UWSOFT_ON_BASE lib/wsoft_esp/src/$f | diff -q - <(git -C ../weighsoft.wsoft show cfcd012:esp/src/$f); done
```

## 2. Bench steps (laptop, not the cloud container)

1. `pio test -e native` — host tests (trade logic, live view, settings). Needs `g++` on PATH.
2. `pio run -e wsoft_waveshare43` — builds on Arduino-ESP32 3.1.1 (bench, 26 Oct 2026:
   RAM 16.6 %, flash 35.0 %; ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, AsyncMqttClient 0.9.0,
   ArduinoJson 6.21.6, WebSockets 2.7.3, ESP32_Display_Panel 1.0.5). The framework needed small
   core-3 fixes (handler casts, WiFi.h / esp_mac.h includes, readIP, mDNS rename), all also
   valid on core 2. The first build downloads toolchains (~45 min); rebuilds take 3–7 min. Also build two other envs to prove nothing else broke:
   `pio run -e nodemcu_esp32 -e esp32s3` (`wsoft_esp` is in `lib_ignore` for every env but
   this one).
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

7. Hub pairing (if needed): web page **W-Soft > Hub** (admin): paste the key from the Hub
   (Devices > the device > New pairing code > Copy code), press **Connect**. The board
   restarts and the status chip should turn `HUB ONLINE`. Or over USB: `HUB PAIR <code>`
   then `HUB SAVE`. `GET /rest/wsoftHub` shows the Hub address and ids only; the key is
   write-only. The key holds the board's signing secret, so only admins see this tab.

## 3. USB commands (UART0, 115200 8N1)

| Command | What it does on this build |
|---|---|
| `APPROVAL STATUS` | Prints ON/OFF, audit counter, trade check, next sequence, queue size |
| `APPROVAL LOG` | Prints the last approval switches |
| `APPROVAL ON <technician> <reason>` | Switches approval on (logged with `where=usb`, audit counter +1). Same rules as section 4b: refused with `APPROVAL_ERR SETUP_INCOMPLETE ...` while the RS485 setup is not complete |
| `APPROVAL OFF <technician> <reason>` | Switches approval off (logged with `where=usb`, audit counter +1) |
| `HUB PAIR <code>` | Checks a Hub pairing code and holds it |
| `HUB SAVE` | Saves the held pairing code to flash and restarts |
| `HUB CLEAR` | Forgets the Hub settings and restarts |
| `HUB WIFI` / `HUB PASS` / `HUB OTA` | **No-op here.** Replies `HUB_ERR not used on this build`. Use the web pages |

Approval can be switched from the screen, the browser, the Hub and this USB line; Auto save
from the first three. All go through `switches.h` and the same rules (section 4b). There is still no MQTT or
BLE write path.

## 4. Web, WebSocket and MQTT

| Path | Who | What |
|---|---|---|
| `GET /rest/wsoftStatus` | logged-in user | Live view (JSON below) |
| `POST /rest/djb` | admin | One DJB register read or write (below). The screen task runs it on Serial1 |
| WebSocket `/ws/wsoftStatus` | logged-in user | Same JSON, pushed when it changes |
| `GET/POST /rest/wsoftSettings` | admin | Options; a bad topic or name returns 400 and keeps the old values |
| `GET /rest/wsoftSwitches` | logged-in user | Approval and Auto save state, whether Approval can go ON, PIN state, the last 8 approval log lines |
| `POST /rest/wsoftSwitches` | admin | `{"switch":"approval"\|"auto_save","on":true\|false,"reason":"text"}` (section 4b) |
| `POST /rest/wsoftPin` | admin | `{"pin":"1234"}`: sets the screen PIN. Never sent back or logged |
| `GET /rest/wsoftScaleSetup` | logged-in user | The board's part of the RS485 setup, what is missing, and the card's e as last read |
| `POST /rest/wsoftScaleSetup` | admin | `{"board":{...},"reason":"text"}`; a key that is not sent keeps its value |
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
`NO_CARD`, "No weighing card: the scale is on RS232.") in RS232 mode, and (409 `not_linked`)
while the scale is not linked, and every write is refused (409 `approval_on`)
while approval is on: an `ad` write is a calibration (0 zero, CW > 0 load) and the scale
parameters are metrological. Out-of-range writes return 400 `ad_range` / `range`: `ad`
0..1000000, `unit` 0..3, `decimal` 0..4, `address` 1..10 (the link scan range), `baud`
0..2, `byte_order` 0 and `com1_mode` 1 (the poll needs both). Writing `address` or `baud`
drops the link until the scan finds the scale again. After a write the poll re-reads unit,
decimals and division at once, and every successful write adds 1 to `setup_audit`.

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

## 4b. Switches, screen PIN and the RS485 scale setup

Errors are 409 (or 400 for a bad shape) with `{"error":"CODE","message":"plain words"}`.

**Switches.** The board holds the true state; the screen, the web page and the Hub show it.

- Approval needs a reason: 1 to 80 printable characters (ASCII 0x20 to 0x7e) after trimming.
  Auto save takes one but does not need it.
- Approval cannot go ON while the RS485 setup is not complete (`SETUP_INCOMPLETE`). On RS232
  the indicator owns the setup, so this check does not apply.
- Codes: `ALREADY`, `SETUP_INCOMPLETE`, `REASON_REQUIRED`, `REASON_TOO_LONG`, `WHO_REQUIRED`,
  `BUSY` (the screen task did not take the change within 1 s, or another change is in the
  box: **not applied**) and `UNCONFIRMED` (taken, but no answer 5 s later: it **may** have
  been applied; read the state again).
- Every approval switch is one log line in NVS `approval`:
  `<audit> <time> OFF->ON by=<user> where=<screen|browser|hub|usb> why=<reason>`.
  A name over 64 characters is cut and ends in `~`. Auto save switches go to the USB log
  as `SWITCH auto_save ...`.
- The screen task applies every change (it is the only flash writer). Other tasks hand it
  the change through a one-slot mailbox with a ticket each, so no caller gets another's
  answer, and wait for it. Once taken, a change is waited for until it is done.

**Screen PIN.** 4 to 6 digits, set by an admin in the browser. Stored as SHA-256 of a random
16-byte salt plus the PIN (NVS `pin`). After 5 wrong tries the pad locks for 300 s; the
count is kept across a restart, so 5 stored wrong tries lock the pad again for 300 s after a
restart, until the right PIN. The USB log says `PIN wrong try n of 5`, never the digits.
Setting the PIN answers 409 `BUSY` if a try on the screen holds it for more than 0.5 s.

**Screen MENU.** Tap the bottom left (GROSS) box. The switches page has Approval, Auto save,
TARE and PRESET TARE (RS485 only), and BACK. Approval opens a number pad (DEL, OK, CANCEL);
the page title shows one mark per digit. After 30 s with no tap the screen goes back to the
live weight. Taps log `TOUCH screen=x,y` on USB.

**RS485 scale setup** (NVS `scale`). Board keys:

| Key | Rule |
|---|---|
| `sa_number` | 1 to 32 printable characters |
| `max_kg`, `min_kg` | kg text, a multiple of e, `max > e`, `min >= e`, `min < max` |
| `blank_below_e`, `blank_above_e` | counts of e, 0 to 100 (`blank_above_e` defaults to 9) |
| `stable_samples` | 3 to 20 (default 3) |
| `tare_enabled`, `tare_max_kg`, `preset_tare_kg` | `tare_max <= max`, `preset <= tare_max` |

- e = the card's division in its decimals and unit (register 41307, read by the poll).
- Codes: `SETUP_LOCKED` (approval ON or RS232), `SETUP_ORDER`, `SETUP_NUMBER`, `SETUP_TEXT`,
  `SETUP_RANGE`, `REASON_REQUIRED`, `REASON_TOO_LONG`, `BUSY` (the setup lock was not free
  within 0.5 s).
- `complete` = `sa_number`, `max_kg`, `min_kg` and `blank_below_e` are set and the card has
  been read. `missing` lists what is not.
- Every saved change adds 1 to `setup_audit`. Saving clears the tare.
- On RS485 the screen and the saver hold the weight: `OUT OF RANGE` (below
  `-blank_below_e` x e, or above Max + `blank_above_e` x e, on the gross), `BELOW MIN` (a
  NET load above 0 but under Min; with a tare on this is stricter than the gross, **to
  confirm against the SA rule**), or moving until `stable_samples` readings match. With a
  complete setup, a reading that is not an exact kg value is `OUT OF RANGE` too. Such a
  weight is not for trade and is not saved (below Min still saves while approval is off).
- Tare (RS485): TARE takes the stable gross, TARE again clears it, PRESET TARE uses the
  saved preset. A tared weighing goes to the Hub as gross / tare / net with basis `NT`.

**Hub commands.** While the Hub is online the board posts to `POST /api/v1/ingest/commands`
every 10 s, signed like the heartbeat. The answer is checked against
`scale-models-hub-command-v1` before anything is applied. Commands run in id order through
the same rules (`where=hub`); the last applied id is kept in NVS `hub` (key `cmd`), and is
removed whenever the pairing changes (web Connect or Forget, USB `HUB PAIR` + `HUB SAVE`,
`HUB CLEAR`). `BUSY` is asked again at the next pull; `UNCONFIRMED` counts as handled.
The results waiting to go to the Hub are kept in RAM only: after a restart they are lost,
and the Hub marks those commands expired. A switch from any place sends a heartbeat at
once, with `auto_save` and `scale_setup_complete` in `health`.

**Update note.** Auto save moved from W-Soft > Settings to the switches (NVS `weigh`). At
the first boot after the update the board reads the old `auto_save` once from
`/config/wsoftSettings.json` and keeps it.

## 5. How it runs

- The W-Soft screen loop (Modbus poll, up to ~600 ms, plus panel draw, up to ~1 s) runs in its
  own FreeRTOS task on core 1 (8 KB stack, 5 ms pause per pass). The Arduino loop keeps the web
  server, MQTT and the framework moving.
- After every poll the screen task writes a snapshot under a spinlock. `WSoftStatusService::loop()`
  copies it and publishes only when something changed, at most once per publish interval.
- The framework owns Wi-Fi, NTP and OTA (`-D WSOFT_ON_BASE`). Approval, the Hub queue, the
  weighings, the PIN and the scale setup use NVS (`Preferences`, namespaces `approval`, `hub`,
  `weigh`, `pin` and `scale`) beside the framework's LittleFS.
- RS485 is `Serial1` RX15/TX16 and belongs to W-Soft. The Serial1 services of this branch are
  not created on this board.

## 6. Known gaps

- Flashed on the bench board (26 Oct 2026, COM10, no backup of the old firmware). The first
  flash boot-looped: on core 3 `sntp_stop()` asserts without the lwIP core lock, and the
  framework calls it at boot when there is no Wi-Fi. `NTPSettingsService` now takes the lock
  (same pattern as the core's `configTime()`), and `WiFiSettingsService` skips
  `WiFi.mode(WIFI_MODE_MAX)`, which core 3 rejects. After that: 0 reboots in 105 s.
- The board's COM10 port is the ESP32-S3 USB-Serial-JTAG (boot ROM and `ESP_LOG` only). The
  app's `Serial` log goes to UART0, the board's other USB-C port.
- Framework OTA is off (`FACTORY_OTA_ENABLED=false`). If it is switched on later, the screen
  keeps polling during an upload: `hub_ota_busy()` only knows W-Soft's own OTA.
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
