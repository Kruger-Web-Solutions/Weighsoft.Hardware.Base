# AGENTS.md — start here (humans and AI agents)

Read this first, then [docs/BRANCH-MAP.md](docs/BRANCH-MAP.md).

## What this repo is

`Weighsoft.Hardware.Base` is the shared ESP8266/ESP32 firmware base (ESP8266-React: C++ backend,
React web UI). **One base, many devices: each device type lives on its own long-lived branch.**
`master` is only the framework. Never assume device code is on `master`.

- Which branch builds which device → [docs/BRANCH-MAP.md](docs/BRANCH-MAP.md)
- How to branch and merge → [docs/INTEGRATION-WORKFLOW.md](docs/INTEGRATION-WORKFLOW.md)
- W-Soft firmware → branch `wsoft-esp32s3` (main W-Soft line). The firmware flashed on
  the site boards on 18 Nov 2026 is tag `wsoft-s3-2026-11-18`.
- How to flash W-Soft onto an ESP32-S3 → `Kruger-Web-Solutions/Weighsoft.WSoft`,
  `docs/esp/esp32s3-wsoft-flash.md`

## Rules

1. Build on existing code. Never rewrite a device branch from scratch.
2. Never invent a protocol, pinout or calibration. Ask the owner.
3. No secrets in code, commits, PRs or docs. This repo is **public**. Run gitleaks before
   every push.
4. Work on a task branch off the device branch, then PR back into that device branch. Delete
   the task branch once it is merged.
5. Stack: ESP C++ firmware, React UI. No new Python.
