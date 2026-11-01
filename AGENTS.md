# AGENTS.md — start here (humans and AI agents)

Read this first, then [docs/BRANCH-MAP.md](docs/BRANCH-MAP.md).

## What this repo is

`Weighsoft.Hardware.Base` is the shared ESP8266/ESP32 firmware base (ESP8266-React: C++ backend,
React web UI). **One base, many devices: each device type lives on its own long-lived branch.**
`master` is only the framework. Never assume device code is on `master`.

- Which branch builds which device → [docs/BRANCH-MAP.md](docs/BRANCH-MAP.md)
- How to branch and merge → [docs/INTEGRATION-WORKFLOW.md](docs/INTEGRATION-WORKFLOW.md)
- W-Soft weigh screen (Waveshare ESP32-S3 4.3) → [docs/WSOFT-WAVESHARE43.md](docs/WSOFT-WAVESHARE43.md)

## Rules

1. Build on existing code. Never rewrite a device branch from scratch.
2. Never invent a protocol, pinout or calibration. Ask the owner.
3. No secrets in code, commits, PRs or docs. This repo is **public**.
4. Work on a task branch off the device branch, then PR back into that device branch.
5. Stack: ESP C++ firmware, React UI. No new Python.

## Bench devices (31 October 2026)

| Device | Chip | Bench IP | Firmware source |
|---|---|---|---|
| Waveshare 4.3" W-Soft screen | ESP32-S3 | 192.168.1.10 | branch `claude/project-thread-9f4l7x` (PR #27) |
| 4-relay board | **ESP32** | 192.168.1.11 | **separate repo** `Kruger-Web-Solutions/Weighsoft-Relay-Control` (`main`, env `node32s`) |
| ESP-12F 4-relay board (older) | ESP8266 | — | branch `RelayBoardEspBuildIn` |

The bench network is a TOTOLINK router at 192.168.1.1, Wi-Fi `W-Soft` (2.4 GHz). If a device
goes missing from the router, it has probably lost its Wi-Fi settings: it then opens its own
hotspot `ESP8266-React-<id>` at 192.168.4.1, where you can pick `W-Soft` again under
**WiFi Connection**. Bench prototypes keep their factory passwords on purpose (owner's choice).

## Known open items

- The ESP32 relay firmware lives outside this repo. Plan: bring it in as a device branch.
- `ALLinOneEspSerialdevice` and `wsoft-waveshare43` are the same commit; pick one name.
- The 4 `sprint-c/*` branches are fully merged into `RelayBoardEspBuildIn` and can be deleted.
