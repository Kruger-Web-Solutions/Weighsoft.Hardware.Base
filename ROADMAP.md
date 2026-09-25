# Roadmap

Status on **2026-09-24**. Every fact below was checked in git.
Where there is no record of a test, it says so.

**In one line:** work has stalled since about 10 Aug 2026.
`master` is only the base framework. The real device work sits on two branches that were never merged.

---

## What `master` is

- The shared ESP8266 / ESP32 framework: WiFi, hotspot, MQTT, time sync, OTA, login, Bluetooth, web screens.
- One demo: the LED example.
- No device product code. No relay, serial, display or weighing code.
- Last firmware change: `aac7aa0`, 2026-02-18 (WiFi auto-reconnect). Since then only docs (March, April), a `.gitignore` change (May) and a CI change (2026-08-23).

**CI** builds `esp12e` and `node32s`, only for pushes and pull requests to `master`.

- It has been red since run `32662931921` (2026-08-23).
- Cause: the ESP32 build tool needs a Python package (`intelhex`) that was not installed.
- Fix: commit `a4713ae` (installs `intelhex` before the build).
- Proof: the first CI run on the pull request that brings `a4713ae` into `master`. No such run is recorded as of 2026-09-24. Update this line with the result.

---

## The two device lines

Neither is merged into `master`.

| | Relay board + Live Weight | Serial Reader / Writer |
|---|---|---|
| Branch | `RelayBoardEspBuildIn` | `SerialReaderWriter` |
| Chip | ESP8266 (ESP-12F, 4 relays, 2 inputs) | ESP32 only |
| Last commit | 2026-08-10 (`b1d0f27`) | 2026-07-16 (`9c74170`) |
| Commits not on `master` | 71 | 155 (and 21 `master` commits it lacks) |
| Built | Relay screen, Live Weight screens, product list, weigh log, network discovery | Reader / Writer / Diagnostics modes, TFT screen, LCD, remote weight |
| Best evidence | Bench smoke test **17/17 PASS** on the real board (`47a1959`, `4652cb0`). Input buttons signed off 2026-08-10. | Compiles for its ESP32 boards, per its own notes (`docs/superpowers/queue.md`). CI never built it. |
| Automated tests | Weight-sender script tests only. No firmware tests. | None |
| Hardware sign-off | Partial (see below) | **None recorded** |

### Relay board: what is still open

- **Two fixes are built. The last record says they were not flashed** (both commits, 2026-08-10):
  - `017253a` - the on-screen **Next** button now saves the weigh. As of that record, weighs done from the screen were being lost.
  - `834a53a` - download the weigh log as a CSV file.
  - What firmware the board runs now: **UNKNOWN**. Nothing after 2026-08-10 records it.
  - Smoke checks QA-013 and QA-014 stay red until these are flashed.
- **No test shows the 4 relays switching.** The smoke test only sends the current relay state back to the board.
- **Printer ticket never tested** (RT-008). It needs a network receipt printer.
- The human sign-off table in its QA checklist is empty.

### Serial line: what is still open

- No automated tests.
- No recorded pass or sign-off for `SerialReaderWriter` builds, including the TFT and LCD screens.
- Earlier notes show parts of this code running on real boards: a TFT panel checked on the bench (`5d8f1ad`, on `espReadWriteRHYNO`, 2026-04-28) and a debug session against a real board (`docs/task-logs/TASK-display-serial-bridge-network.md`, 2026-04-24). Those are observations, not a pass for the merged `SerialReaderWriter` code.
- The ESP8266 build (`esp12e`) does not build on this line, per its own notes.

---

## Decisions only Rhyno can make

1. **Which device line becomes the main one** and gets merged into `master`?
2. **Which relay contract?** The Scale Models Indicator and this firmware do not match today:
   - The Indicator (`platform/edge/src/scale_models_edge/relay_board.py`) sends `{"slot", "state"}` to `/rest/relayBoard`, or `{"relays": [...]}` to `/rest/relayState`.
   - This firmware only reads `relay1` to `relay4` (true / false). No branch has `/rest/relayState`.
   - Worse: the board ignores the fields it does not know and still answers "OK". So the Indicator would think a relay switched when it did not.
   - The Indicator's health check calls `/rest/systemStatus`. Here that needs a login, so a working board would show as "unreachable".
3. **Keep ESP8266 or not?** The relay board is ESP8266. The serial line is ESP32 only. Dropping ESP8266 means a different relay board chip.
4. **Is relay control without a login OK at a customer site?** Jurien locked "public operator" mode on 2026-08-09: weigh, product list, weigh log and relays / inputs work without a login; settings need a login. The record has no lab-only limit. So anyone on the same WiFi can switch relays and send a weight. Whether that is acceptable at a customer site is still open.
5. **What happens to the older serial branches?** They hold work that `SerialReaderWriter` does not have (counted 2026-09-24): `serial` 15 commits, `serialWriter` 48, `espReadWriteRHYNO` 45. Status: not merged, not decided.
   - `espReadWriteRHYNO` has WiFi fixes with no copy on `SerialReaderWriter`: `45d92e6` (join the strongest 2.4 GHz access point), `e592d87` (better hotspot fallback + network watchdog), `865c42a`, `31cf3d4`, and `c6429bc` (stops a WebSocket crash).
   - Only `serialReader` and `ALLinOneEspSerialdevice` are fully inside `SerialReaderWriter`.

---

## Security: do this first

- **Rhyno: change the WiFi passwords that have been committed to this repository.** This repository is public, so treat every committed password as known to anyone. The list of affected networks is kept in the private Scale Models project records, not here.
- Until they are changed, do not share build output or `compile_commands.json` from any branch. A build copies the factory WiFi password into both.
- Replace the lab login `admin` / `admin` before any board goes to a customer.

---

## Agent work that needs no hardware

- [x] CI fix written (`a4713ae`, `intelhex`). *Not proven until CI runs on a pull request.*
- [x] `.gitignore` rule for device config (`data/config/`) and `compile_commands.json`. **Done for `master` only, once this branch is merged.** No device branch has it. It cannot stop a file git already tracks.
- [ ] Bring that rule to `RelayBoardEspBuildIn` and `SerialReaderWriter`. For the relay line, the `data/config/` part already sits unmerged on branch `security/gitignore-device-config-and-credential-note` (`7ac9bd5`, 2026-08-10).
- [ ] Stop tracking generated build files and device config on the device branches (`git rm --cached`), after the WiFi passwords are changed. Removing a file does not remove it from history.
- [x] Correct docs that named branches which do not exist (`serial2`, `display35`).
- [ ] Make CI build the device branches too. Today it only runs for `master`.
- [ ] Add automated firmware tests. None exist on any branch.
- [ ] `master`'s ESP32 build (`node32s`) has OTA switched on, but its flash layout has no room for OTA. Needs a decision before changing the layout.
- [ ] The interface build prints "Failed to compile" in CI (style warnings). It does not stop the firmware build.

## Needs hands on hardware

- Flash `017253a` + `834a53a` to the relay board and re-run the smoke test. Needs Rhyno's go-ahead.
- Watch all 4 relays switch on the bench.
- Print one real ticket.
- Try the TFT and LCD screens on real panels.

---

## Every branch on GitHub

Checked 2026-09-24 from the fetched remote branches (this roadmap's own working branch left out).
A branch existing does not mean it is in use. Where no record says what a branch is for now, it says "not recorded".

| Branch | Last commit | What git shows |
|---|---|---|
| `master` | 2026-08-23 | Shared framework + LED demo |
| `RelayBoardEspBuildIn` | 2026-08-10 | Relay line. 71 commits not on `master` |
| `security/gitignore-device-config-and-credential-note` | 2026-08-10 | 1 commit (`7ac9bd5`) not on `RelayBoardEspBuildIn`: the `data/config/` ignore rule. Not merged |
| `sprint-c/phase-1-4-discovery` | 2026-08-09 | Fully inside `RelayBoardEspBuildIn` |
| `sprint-c/phase-2-discovery-json-fix` | 2026-08-09 | Fully inside `RelayBoardEspBuildIn` |
| `sprint-c/phase-2-discovery-rest-probe` | 2026-08-09 | Fully inside `RelayBoardEspBuildIn` |
| `sprint-c/phase-2-udp-bind-fix` | 2026-08-09 | Fully inside `RelayBoardEspBuildIn` |
| `SerialReaderWriter` | 2026-07-16 | Serial line. 155 commits not on `master` |
| `pi-usb-reader-and-dashboard` | 2026-07-15 | 1 commit not on `SerialReaderWriter` (`714f79b`, archive of a Pi USB scale reader + Node-RED dashboard). Not recorded |
| `ALLinOneEspSerialdevice` | 2026-06-14 | Fully inside `SerialReaderWriter` |
| `espReadWriteRHYNO` | 2026-05-02 | 45 commits not on `SerialReaderWriter`, incl. WiFi fixes. Not decided |
| `weighingboard` | 2026-04-29 | 11 commits not on `master`. Whether in use: not recorded |
| `display` | 2026-04-29 | 15 commits not on `master`. Its LCD code was ported to `SerialReaderWriter` as `lcddisplay` |
| `serialReader` | 2026-04-29 | Fully inside `SerialReaderWriter` |
| `serialWriter` | 2026-04-29 | 48 commits not on `SerialReaderWriter`. Not decided |
| `growbox` | 2026-03-31 | 1 commit not on `master` (growbox monitor). Not recorded |
| `serial` | 2026-03-29 | 15 commits not on `SerialReaderWriter`. Not decided |
