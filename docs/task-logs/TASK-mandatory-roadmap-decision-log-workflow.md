# TASK: Mandatory roadmap and decision log (“follow the rules”)

**Status:** DRAFT / IN PROGRESS (standing workflow; not a shipped feature)

**Last updated:** 2026-04-21

## Roadmap

### Active workstream (2026-04-21): ESP32-S3 USB flash + COM16

- **Goal:** Flash the firmware that includes **SerialWriter** (same app as default `esp32s3` build; there is no separate PlatformIO project for serialwriter only) using **`env:esp32s3`** onto the ESP32-S3 attached as **COM16**.
- **Done:** `upload_port = COM16` under `[env:esp32s3]`; `python -m platformio run -e esp32s3 -t upload` after `Get-Process python* | Stop-Process -Force` (per `platformio-upload.mdc`). PlatformIO reported **SUCCESS** (~7m20s including npm interface build and first compile).
- **Pending:** User sign-off that the device behaves as expected (web UI, SerialWriter, etc.).

### Active workstream (2026-04-21): Agent verification — `pio run`, `npm test`, `npm run build`

- **Goal:** Run default firmware build (`esp32s3`), Jest (`npm test` in `interface/`), and standalone `npm run build`; record outcomes in this task log; **do not change product code** for findings unless a failure blocks a later step (none did).
- **Done:** `python -m platformio run -e esp32s3` **SUCCESS**; `npm run build` **SUCCESS** when **`CI` is unset** (eslint remains warnings-only); **`npm test`** green — **`endpoints.test.ts`** + **`route.test.ts`** (**6** tests). **`npm audit`** out of scope per user.
- **Pending:** Optional eslint/browserslist cleanup; no action on npm audit per user. Optional: Jest config to transform **`parse-ms`** if **`formatDuration`** tests are desired later.

- **Goal:** Whenever the user says **follow the rules**, maintain a full roadmap and decision log for the **active engineering workstream** until explicit sign-off.
- **Scope:** Task logs under `docs/task-logs/` only; no promotion to official or user-facing docs before sign-off and completed testing (unless the user explicitly requests otherwise).
- **Ongoing:** Apply this workflow on every future task where the trigger phrase is used.

## Decision log

### 2026-04-21 — `esp32s3` upload port COM16 (SerialWriter firmware)

| Field | Detail |
|--------|--------|
| **What was tried** | Set `[env:esp32s3]` `upload_port = COM16` (was `COM11`) while keeping `upload_protocol = esptool` so this env overrides the global `[env]` OTA defaults (`espota` / IP). |
| **Why** | User requested flashing the ESP32-S3 used for this firmware—including SerialWriter—on **COM16**. |
| **Outcome** | `platformio.ini` updated; upload completed successfully to **COM16**. esptool: chip **ESP32-S3 (QFN56) rev v0.2**, **Embedded PSRAM 8MB (AP_3v3)**, USB mode **USB-Serial/JTAG**, flash write verified, hard reset via RTS. Build compiled `SerialWriterService.cpp` / `SerialWriterForwarderService.cpp` as part of the firmware. |
| **Failure / rejection** | N/A for upload step. |
| **What was learned** | SerialWriter ships in the main firmware (`main.cpp`); no separate env name is required beyond `esp32s3`. **Notable build noise (non-blocking):** npm reported many audit vulnerabilities; CRA/eslint **warnings** (e.g. trailing spaces); repeated **`-DARDUINO_USB_MODE` / `CONFIG_ESP_TASK_WDT_TIMEOUT_S` redefined** compiler notes from overlapping defines. |

### 2026-04-21 — Align repository workflow with user mandate

| Field | Detail |
|--------|--------|
| **What was tried** | Read `.cursor/rules/task-roadmap-decision-log.mdc` and `docs/task-logs/README.md` for parity with the user’s stated requirements. |
| **Why** | Confirm the codebase already documents the same constraints so behavior is consistent and traceable. |
| **Outcome** | The rule file and README already require: task file per workstream, roadmap + decision log during the task, every meaningful attempt (including failed/reverted), proposed solution with tradeoffs/risks/evidence, DRAFT until sign-off, no “final” framing before sign-off, and no official/user-facing doc updates until sign-off (unless explicitly requested). |
| **Failure / rejection** | N/A — no conflict found. |
| **What was learned** | No duplicate policy doc is required beyond this activation log; implementation is “follow existing rule + README on trigger.” |

### 2026-04-21 — Agent verification: `pio run -e esp32s3`

| Field | Detail |
|--------|--------|
| **What was tried** | From repo root: `python -m platformio run -e esp32s3` (Windows PowerShell). |
| **Why** | User-requested automated verification of default firmware build path (includes pre-script `npm install` + `npm run build` for interface). |
| **Outcome** | **SUCCESS** in ~72.6s. RAM ~21.6%, Flash ~84.2% of app partition. |
| **Failure / rejection** | None for compile/link. |
| **What was learned** | Same non-blocking frontend noise as prior sessions: **npm audit** reported **76** vulnerabilities (14 low, 20 moderate, 37 high, 5 critical); CRA **“Compiled with warnings”** (eslint + browserslist); no compiler “macro redefined” lines captured in this run’s stdout (may still occur in verbose builds). |

### 2026-04-21 — Agent verification: `npm test` (`interface/`)

| Field | Detail |
|--------|--------|
| **What was tried** | `cd interface; $env:CI='true'; npm test -- --watchAll=false` |
| **Why** | Exercise Jest per `package.json` `test` script. |
| **Outcome** | **FAILED** — exit code **1**. Jest: “No tests found”; 151 files checked, **0** matches for `testMatch` / `testRegex`. |
| **Failure / rejection** | No `__tests__` or `*.spec.*` / `*.test.*` under `interface/src` (by design or omission). |
| **What was learned** | `npm test` is not a green CI gate today unless tests are added or `--passWithNoTests` is adopted intentionally. **No code changes made** per user instruction. |

### 2026-04-21 — User policy: Jest must run real tests; ignore `npm audit`

| Field | Detail |
|--------|--------|
| **What was tried** | User answered follow-up: (1) **`npm test` must be tested** — not `passWithNoTests`-only; (2) **`npm audit`** — ignore. |
| **Why** | Close the verification workstream with explicit product/CI expectations. |
| **Outcome** | Added **`interface/src/utils/__tests__/endpoints.test.ts`** — unit tests for **`extractErrorMessage`** (server message, axios message, default fallback). **`npm audit`** remains explicitly untracked in this task log beyond noting it exists. |
| **Failure / rejection** | N/A. |
| **What was learned** | Pure util tests avoid full-app fetch graphs while still satisfying a strict “tests must exist” gate. |

### 2026-04-21 — Agent verification: `npm run build` (`interface/`)

| Field | Detail |
|--------|--------|
| **What was tried** | `cd interface; npm run build` (standalone, after `pio run` already exercised the same script). |
| **Why** | Confirm production build without PlatformIO wrapper. |
| **Outcome** | **SUCCESS** (exit 0); same **eslint warnings** and **Browserslist: caniuse-lite is outdated** notice as during `pio run`. |
| **Failure / rejection** | None for build completion. |
| **What was learned** | Warning list is stable across runs: trailing spaces in `Layout.tsx`, `LedControlBle.tsx`, `LedControlWebSocket.tsx`, `LedExampleInfo.tsx`; **max-len** on `LedExampleInfo.tsx` L67 and `WiFiStatusForm.tsx` L157. |

### 2026-04-21 — Agent verification (continuation): `routeMatches` tests + `CI` / CRA pitfall

| Field | Detail |
|--------|--------|
| **What was tried** | Added **`interface/src/utils/__tests__/route.test.ts`** (3 cases for **`routeMatches`**). Prototyped **`time.test.ts`** against **`formatDuration`** / **`formatLocalDateTime`** — **deleted** because **`parse-ms`** is published as **ESM** and Jest (current CRA setup) throws **Unexpected token 'export'** when importing `time.ts`. Ran **`CI=true npm test`** then **`npm run build`** in the **same** PowerShell session: CRA **failed** (“Treating warnings as errors because `process.env.CI = true`”). Re-ran **`npm run build`** and **`python -m platformio run -e esp32s3`** with **`CI` removed**: both **SUCCESS** (~108s firmware build; flash **~84.2%**). |
| **Why** | User asked to continue automated testing; expand Jest coverage without fragile full-app mounts. |
| **Outcome** | **2** test files, **6** passing tests. Firmware + interface pipeline green when **`CI` is not set** during `npm run build`. |
| **Failure / rejection** | **`time.ts`**-based tests rejected for now (dependency on **`parse-ms`** ESM). |
| **What was learned** | Do not chain **`npm test`** with **`CI=true`** and **`npm run build` / `pio run`** in one shell without **`Remove-Item Env:\CI`** (or new session); CRA promotes eslint warnings to **errors** under **`CI`**. |

## Debug checklist (issues observed; triage status)

1. **`npm test`**: ~~Exit **1** — no tests~~ **Resolved:** `interface/src/utils/__tests__/endpoints.test.ts` + **`route.test.ts`** (**6** tests total).
2. **`npm audit`**: **76** vulnerabilities reported during `npm install` — **explicitly ignored per user** (no tracking or fixes in this workstream).
3. **Browserslist**: `caniuse-lite is outdated` — suggests `npx browserslist@latest --update-db` (informational / tooling hygiene).
4. **ESLint (CRA build)**: `no-trailing-spaces` — `src/components/layout/Layout.tsx` L35; `src/examples/led/LedControlBle.tsx` L11, L15, L36; `src/examples/led/LedControlWebSocket.tsx` L28, L30, L49, L55, L70, L92, L112, L133; `src/examples/led/LedExampleInfo.tsx` L13, L17, L44, L83.
5. **ESLint `max-len` (>140)**: `src/examples/led/LedExampleInfo.tsx` L67 (150 chars); `src/framework/wifi/WiFiStatusForm.tsx` L157 (158 chars).
6. **Flash headroom**: Firmware flash usage **~84.2%** of configured app partition — not an error, but worth tracking for future growth.
7. **`CI` + CRA**: If **`CI=true`** is left set after Jest, **`npm run build`** (and PlatformIO’s **`npm run build`** step) **fail** on current eslint **warnings** (CRA treats them as errors). **Mitigation:** unset **`CI`** before production build, or fix eslint items (4–5 above).

## Current approach (not final until sign-off on a given task)

- **Why selected:** Matches user non-negotiables and existing project rules.
- **Tradeoffs:** Extra overhead on small tasks; benefit is full reconstructability and honest record of dead ends.
- **Risks:** Forgetting to update the task file mid-stream; mitigated by treating updates as part of the same cadence as code changes when the trigger is active.
- **Validation:** Policy alignment verified by file review. For the COM16 flash workstream: **`esp32s3` upload SUCCESS** on hardware (see decision log above). Functional tests on the board are still **pending user sign-off**. For the agent verification workstream: **`pio run -e esp32s3`** and **`npm run build`** succeed with **`CI` unset**; **`npm test`** (**6** tests: **`extractErrorMessage`**, **`routeMatches`**); **`npm audit`** ignored; see debug checklist **item 7** for **`CI`** vs CRA.

## Sign-off

- [ ] User explicitly signs off on this standing workflow document (optional; workflow remains in effect per user instruction regardless).
