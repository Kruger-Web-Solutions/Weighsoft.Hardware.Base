# pre-upload script: clear the otadata partition before USB-flashing
#
# Why: when an ESP32 device has previously been OTA-updated, the bootloader
# uses the `otadata` partition to decide which app slot (ota_0 vs ota_1) to
# boot. esptool USB uploads write to `ota_0` (offset 0x10000) by default, but
# `otadata` may still point at `ota_1` from a prior OTA — so the chip boots
# the OLD firmware and you can't tell why your fresh flash didn't take.
#
# This script:
#   1. Skips ESP8266 (different chip, no esp32-style OTA).
#   2. Skips espota uploads (OTA handles its own slot management).
#   3. Reads the env's partition CSV, finds the `otadata` row if present.
#   4. Adds a pre-upload action that runs `esptool erase_region <off> <size>`
#      so the bootloader falls back to ota_0 (where esptool just wrote).
#
# If the partition layout has no `otadata` (e.g. single-factory node32s),
# this is a silent no-op.
#
# Triggered via `extra_scripts = pre:scripts/clear_otadata.py` in platformio.ini.

Import("env")  # noqa: F821  — provided by PlatformIO at runtime
import os
import csv

PLATFORM = env.PioPlatform()  # noqa: F821

# Skip ESP8266 entirely — different OTA model.
if PLATFORM.name == "espressif8266":
    print("[clear_otadata] ESP8266 platform — nothing to do.")
else:
    upload_protocol = env.GetProjectOption("upload_protocol", default="esptool")  # noqa: F821

    if upload_protocol == "espota":
        print("[clear_otadata] OTA upload — bootloader will pick the new slot. Skipping.")
    else:
        partitions_file = env.GetProjectOption("board_build.partitions", default="")  # noqa: F821

        if not partitions_file:
            print("[clear_otadata] No board_build.partitions configured — skipping.")
        else:
            partitions_path = os.path.join(env["PROJECT_DIR"], partitions_file)  # noqa: F821

            if not os.path.exists(partitions_path):
                print(f"[clear_otadata] Partition CSV not found at {partitions_path} — skipping.")
            else:
                otadata_offset = None
                otadata_size = None
                with open(partitions_path, "r") as fh:
                    reader = csv.reader(fh)
                    for row in reader:
                        if not row or row[0].lstrip().startswith("#"):
                            continue
                        # Columns: name, type, subtype, offset, size, flags
                        if len(row) < 5:
                            continue
                        name = row[0].strip()
                        if name == "otadata":
                            otadata_offset = row[3].strip()
                            otadata_size = row[4].strip()
                            break

                if otadata_offset is None:
                    print("[clear_otadata] No otadata partition in this layout — skipping.")
                else:
                    print(f"[clear_otadata] otadata at {otadata_offset} size {otadata_size} — will erase before upload.")

                    def erase_otadata(source, target, env):  # noqa: F811
                        upload_port = env.subst("$UPLOAD_PORT")
                        if not upload_port:
                            print("[clear_otadata] No UPLOAD_PORT resolved — skipping erase.")
                            return
                        # Build the esptool command using whatever esptool the platform provides.
                        esptool_path = env.subst("$PYTHONEXE") + " " + os.path.join(
                            PLATFORM.get_package_dir("tool-esptoolpy") or "", "esptool.py"
                        )
                        chip = env.BoardConfig().get("build.mcu", "esp32")
                        cmd = (
                            f'{esptool_path} --chip {chip} --port {upload_port} '
                            f'--baud 460800 erase_region {otadata_offset} {otadata_size}'
                        )
                        print(f"[clear_otadata] Running: {cmd}")
                        env.Execute(cmd)

                    env.AddPreAction("upload", erase_otadata)  # noqa: F821
