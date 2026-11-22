# Windows USB drivers (vendored for ESP32-S3 bench)

These archives were downloaded into this folder so operators can install drivers **offline** when Device Manager shows **Unknown device** or generic **USB Serial Device** without a working `esptool` session.

| Archive | Vendor | Typical hardware IDs | Install hint |
|---------|--------|----------------------|--------------|
| `ESP32.USB.JTAG-v6.1.7600.16386.zip` | Espressif | `VID_303A` (USB JTAG / serial / composite on native USB) | Extract → follow `README` or `.inf` inside the package; use **Update driver** → **Browse** → pick extracted folder for the failing device. |
| `CP210x_Universal_Windows_Driver.zip` | Silicon Labs | `VID_10C4&PID_EA60` (CP210x UART bridge on many DevKit **UART** USB ports) | Extract → run `silabser.inf` **Install** (right-click) or use the installer if present in the zip. |

**Not vendored here (install manually if Hardware Ids show WCH):** WCH **CH340 / CH343** drivers — official downloads: [WCH CH343 serial driver](https://www.wch-ic.com/downloads/CH343SER_ZIP.html) and related pages on [wch-ic.com](https://www.wch-ic.com/).

**Sources (authoritative URLs used for download on 2026-04-24):**

- Espressif: `https://github.com/espressif/esp-win-usb-drivers/releases/download/ESP32-USB-JTAG_v6.1.7600.16386/ESP32.USB.JTAG-v6.1.7600.16386.zip`
- Silicon Labs: `https://www.silabs.com/documents/public/software/CP210x_Universal_Windows_Driver.zip`

After installation: unplug the board, wait a few seconds, plug back in, confirm **no yellow warning** on the Espressif / CP210x / serial entries in Device Manager, then retry `esptool … flash_id` and PlatformIO upload.
