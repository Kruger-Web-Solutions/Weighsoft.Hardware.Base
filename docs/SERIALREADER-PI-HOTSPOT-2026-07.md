# serialReader on a Pi hotspot (July 2026): what was done

A record of uncommitted work found on 18 Nov 2026 in a local clone of this branch
(`serialReader` @ `165f718`) on the Weighsoft bench PC. Nothing here was pushed before.
No passwords, network names or IP addresses of a real site are in this file.

## What it was for

Weigh boards running the `serialReader` firmware were set up to join a **Raspberry Pi
hotspot** instead of the normal site Wi-Fi. The Pi then took their weights and showed them
in Node-RED. This is the same setup as the `pi-usb-reader-and-dashboard` branch (Pi + Node-RED
dashboard, 15 Jul 2026).

```
  scale ──serial──► ESP (serialReader) ──Wi-Fi──► Pi hotspot (NetworkManager, 10.42.0.x)
                         │                              │
                         └── Weight Forwarder ──ws://──► Node-RED on the Pi
```

## What was changed in the clone (9 Jul 2026)

| File | Change | Committed now? |
|---|---|---|
| `factory_settings.ini` | Factory Wi-Fi name changed to the Pi hotspot's network, password left as it was | **No.** The Wi-Fi defaults are blanked instead (see below). |
| `data/config/wifiSettings.json` | Wi-Fi name + password pre-seeded into the board's file system image (`.pio/build/esp32s3/littlefs.bin`, built 9 Jul) | **No.** Holds a password. Now git-ignored. |
| `src/idf_component.yml` (+ `.orig`, identical) | `idf: '>=5.1'`, made 18 Jun 2026 by a build attempt with the ESP-IDF component manager. Not used: this branch builds with `framework = arduino` on `espressif32`. | **No.** Now git-ignored. |

## Helper scripts (kept outside the repo, not committed)

A `tools/` folder next to the clone holds 19 scripts from 9 Jul 2026. They have Wi-Fi and admin
passwords as built-in defaults, and some use Python (`python3 -c`), so they are **not** in this
repo. What they did:

| Script | What it does |
|---|---|
| `find_esp*.sh`, `find_esp*.ps1` | Scan the hotspot or LAN for boards that answer `GET /rest/wifiStatus` |
| `configure_esp.sh` / `.ps1` | Sign in (`POST /rest/signIn`), set Wi-Fi (`/rest/wifiSettings`) and the Weight Forwarder (`/rest/weightForwarder`) to the Pi's Node-RED WebSocket |
| `configure_all_esps.sh`, `setup_connected_esp.sh`, `wait_and_configure_esps.ps1` | Do the above for every board found, or wait for a board by MAC |
| `add_dhcp_reservation.sh`, `watch_hotspot_esps.sh` | Give each new board a fixed address on the hotspot (from `10.42.0.101`) via NetworkManager's dnsmasq |
| `weighsoft-esp-watcher.service` / `.timer` | systemd unit that runs the watcher on the Pi |
| `revert_esp_weighsoft.ps1`, `revert_production_esps.ps1` | Move boards back from the hotspot to the normal site Wi-Fi |
| `test_nodered_ws.sh` / `.js` | Send a test weight to the Node-RED WebSocket |

If these are needed again, port them to Node.js with the passwords taken from a git-ignored
`.env`, then add them here.

## What this PR changes

1. `factory_settings.ini`: the factory Wi-Fi name and password are now **empty**, like on
   `master`, `SerialReaderWriter` and `wsoft-esp32s3`. A real password had been committed
   here since 25 Mar 2026 (`3dfbeb3`). A board built from this branch now starts its own
   hotspot until Wi-Fi is set on its web page. Put a site's Wi-Fi in locally for a build and
   never commit it.
2. `.gitignore`: device runtime config with secrets (`wifiSettings.json`,
   `securitySettings.json`, `otaSettings.json`, `mqttSettings.json`) and the
   `idf_component.yml` leftovers.
3. This file.

Blanking the value does **not** remove it from git history. See [SECURITY.md](SECURITY.md):
the password must be changed on the network itself.
