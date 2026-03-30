# OTA (Over-The-Air) Firmware Upload

OTA updates **are supported** and have been used successfully on this project. Use this guide when uploading firmware wirelessly to ESP32-S3 (or other OTA-capable envs).

## When OTA Is Available

- **ESP32-S3** (env `esp32s3`): ✅ OTA supported — 8MB flash, partition scheme `partitions_ble_ota.csv` with dual app slots.
- **node32s** (4MB ESP32 with BLE): ❌ OTA not supported — see [CONFIGURATION.md](CONFIGURATION.md#ota-limitations-on-4mb-esp32-with-ble).

## Device-Side Setup

1. Flash the device at least once via USB so it runs the firmware and joins WiFi.
2. Open the device web UI → **System** → **OTA SETTINGS**.
3. Ensure **Enable OTA Updates?** is checked.
4. Set **Port** to **8266** (default).
5. Set **Password** to **esp-react** (or your chosen password; must match `upload_flags` in `platformio.ini`).
6. Click **SAVE**.

The device must be powered on, on the same network as your PC, and have OTA enabled for uploads to succeed.

## platformio.ini (ESP32-S3)

For OTA upload, the **esp32s3** env must use `espota` and match the device OTA port and password:

```ini
; Comment out USB upload when using OTA
;upload_protocol = esptool
;upload_port = COM10

; OTA upload
upload_protocol = espota
upload_port = 192.168.3.115
upload_flags =
  --port=8266
  --auth=esp-react
```

- **upload_port**: Device IP address (find it in the web UI or your router). Example: `192.168.3.115`.
- **--port**: Must match the **Port** in the device’s OTA Settings (default **8266**).
- **--auth**: Must match the **Password** in the device’s OTA Settings (default **esp-react**).

## Upload Command

From the project root:

```powershell
python -m platformio run -e esp32s3 -t upload
```

For USB upload, comment the OTA block in `platformio.ini` and uncomment the `esptool` / `upload_port = COMx` lines, then run the same command (or use the PlatformIO IDE Upload button).

## Troubleshooting

| Symptom | Check |
|--------|--------|
| "No response from the ESP" | Device IP correct; device on and on same network; OTA enabled in UI; port 8266 and password match `upload_flags`. |
| Wrong port | Web UI **OTA SETTINGS** shows the actual port (default 8266). Use that in `--port=`. |
| Auth failure | Password in **OTA SETTINGS** must match `--auth=` in `platformio.ini`. |

## References

- OTA API: [API-REFERENCE.md](API-REFERENCE.md) — GET/POST `/rest/otaSettings`
- Build and feature flags: [CONFIGURATION.md](CONFIGURATION.md)
- USB upload / COM port: [.cursor/rules/platformio-upload.mdc](../.cursor/rules/platformio-upload.mdc) (kill Python before USB upload if port is busy)
