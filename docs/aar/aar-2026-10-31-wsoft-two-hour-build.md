# AAR: the two-hour W-Soft firmware build (31 Oct 2026)

Trigger: decision visibly failed (an estimate of "3-10 min" became 2 h 04 min) and the same
problem recurred (a full rebuild had already happened at 13:16 the same day).
Blameless: the causes below are gaps in the tooling and the process, not anyone's choices.
All times are SAST (UTC+2) on the bench laptop. Evidence: the build logs in the laptop
session's scratchpad (`build-*.log`), `git reflog`, and the PlatformIO source on the laptop.

## 1. Summary

The save-weighing firmware (auto-save, touch SAVE, web Save) was pulled at 14:32 and was
expected to build in 3-10 minutes, because earlier small changes had built in 81-133 s. It
was flashed at 16:37: 2 h 04 min later. Three things stacked: the feature added four new
source files, which makes PlatformIO delete the whole build folder (a full rebuild of about
320 files); a second build was started in the same folder at 14:58 and collided with the
first; and the first build was killed at about 15:55 by an agent session restart, so its
progress was lost and a third, nearly full build ran from 16:05 to 16:36.

Impact: about 2 h of the owner's bench time spent waiting, 8+ status questions, one
collided build, one killed build, and roughly 87 + 32 = 119 min of compile time for work a
single clean full build does in about 30 min (and an incremental build in 2 min).

Lesson: on this laptop a W-Soft build is either 2 min or 30 min, and which one is decided
before it starts (new or removed source files, or a platformio.ini change, mean 30). Say
which before starting, and never run two builds in one folder.

## 2. Timeline

| Time | Event | Evidence |
|---|---|---|
| 12:30-12:33 | Calibrate-tab build, 2 files compiled, 133 s | build-41d345a.log |
| 12:59-13:01 | Calibration-ack fix build, 3 files, 82 s | build-ackfix.log |
| 13:12 | Pull of 131957a/49912cb: new `wsoft_waveshare43_ota` env (new build folder) | reflog |
| 13:16-13:47 | Full build of the new env, 320 files, 1811 s (first full rebuild of the day) | build-ota.log |
| 14:08 | Pull of 9d2199c plus a3a9b72/e748e62 (OTA port in platformio.ini, then reverted) | reflog |
| 14:32 | Pull of the save-weighing commits (253bff7): adds save_logic.cpp/.h and save_queue.cpp/.h | reflog; `git diff --name-status 49912cb 253bff7` |
| 14:32 | Agent build starts; status message says "only the changed files recompile, about 3-10 min" | build-save.log; project thread |
| 14:58 | A second `pio run` of the same env starts in the same folder (owner's PowerShell); it fails with `ar: NetworkClient.cpp.o: No such file or directory` | owner's pasted output |
| 15:43 | Agent reports "full rebuild, 234 of about 320 files" | project thread |
| 15:53 | Cursor's frozen build from 10:18 killed on request (14 processes; 0 CPU each, so not a cause) | taskkill output |
| ~15:55 | Agent session restarts; the background build is killed (exit 3221225786 on 8 objects, before linking) | build-save.log; task notice "did not finish before the previous session ended" |
| 16:05-16:36 | Restarted build compiles 293 files, 1897 s, SUCCESS | build-save2.log |
| 16:37 | Flashed over USB COM10, hash verified, ESP back up | esptool output |
| 16:50-16:52 | Next change (3de1648) builds incrementally in about 2 min, as expected | firmware.bin and .sconsign timestamps |

## 3. Root cause (five whys)

1. Why did it take 2 h? Two near-full builds (87 min, then 32 min) ran instead of one
   2-minute incremental build.
2. Why a full build at 14:32? PlatformIO deletes the build folder when the project
   checksum changes, and the checksum covers the list of `.c/.cpp/.h` files under
   `include`, `src` and `lib` (`platformio/project/helpers.py`, `compute_project_checksum`;
   `platformio/run/helpers.py`, `clean_build_dir` calls `fs.rmtree`). The feature added
   four new files.
3. Why was that not expected? The team's working rule was only "changing platformio.ini
   forces a full rebuild" (commit e748e62's message). Added files were not in the rule, so
   the estimate was given from the size of the change, not from the checksum inputs.
4. Why did the full build then run twice? The first was disturbed by a second build in the
   same folder and then killed by the session restart; SCons records finished objects in
   `.sconsign*.dblite` when the run ends, so a killed run loses that record and the next run
   recompiles nearly everything (293 files).
5. Why could two builds run in one folder, and a build die with its session? Nothing
   enforces one build per folder (PlatformIO has no lock), and the agent started its build
   as a child of its own session, so a restart killed it with no notice to the owner.

Root cause: the bench build has no guard for its two expensive states. Nothing says in
advance "this will be a full rebuild", nothing stops a second build in the same folder, and
nothing keeps a build alive across an agent restart.

## 4. Contributing factors

- **Which record reported success while this was happening?** The agent's own status
  messages: "only the changed files recompile" (14:32) and later "nearly done" were
  confident while the build was full and then killed. The live-log window was honest, but
  it showed a compile stream, not "file N of about 320". The estimate was memory-based,
  not checked against the checksum inputs.
- The owner's chained command (`git pull; pio run; esptool`) is a reasonable habit; the
  guide did not say "check that no other build is running first".
- Each framework file takes about 20-40 s of CPU on this laptop, so a full build is about
  30 min (1811 s and 1897 s measured). Cause not measured: Windows Defender scanning of
  `.pio` and `~/.platformio` is a common cause and is untested here.
- The cloud session cannot run `pio run`, so every firmware build lands on the bench
  laptop.
- Cursor's frozen build was suspected; measured at 0 CPU per process, it was not a factor.

## 5. What went well

- The 98 host tests ran in about a minute before the firmware build, so no logic bug was
  waiting at the end of the long build. Keep: host tests first, firmware second.
- The incremental path works: 81 s, 133 s, and the next change at 16:50 built in about
  2 min. Keep builds in one folder per branch so the cache survives.
- The USB flash took 20 s with a verified hash, and the agent checked the device answered
  the new endpoint (`/rest/wsoftSave` 401) before saying "done".
- The live-log window let the owner see real progress instead of trusting messages.

## 6. Action items

| # | Action | Owner | Due | Tracking |
|---|---|---|---|---|
| 1 | Add a "before you build" note to the bench guide: a full rebuild (about 30 min) happens when a `.c/.cpp/.h` file is added or removed, or `platformio.ini` changes; otherwise about 2 min. One build per folder at a time. | Claude (laptop session) | 2026-10-31 (done in this change) | docs/WSOFT-WAVESHARE43.md section 2 |
| 2 | Add a build wrapper `scripts/wsoft-build.ps1` that (a) refuses to start while another `platformio run` uses this folder, (b) prints FULL or INCREMENTAL from the checksum before compiling, (c) runs detached with its own log so an agent restart cannot kill it. Prove (a) can fail: start two, see the second refused. | Claude (laptop session) | 2026-11-03 | docs/missions/2026-10-31-wsoft-build-guard.md A2 |
| 3 | Agents give a build estimate only after checking the checksum inputs (new/removed source files, platformio.ini diff) since the last build; status lines say "file N of about M". | Claude (laptop session) | 2026-11-01 | docs/missions/2026-10-31-wsoft-build-guard.md A3 |
| 4 | Measure one full build with Windows Defender exclusions for `C:\Users\User\.platformio` and the project `.pio` folder (a security setting, so the owner applies it), and record the time. | Jurien | 2026-11-04 | docs/missions/2026-10-31-wsoft-build-guard.md A4 |

GitHub issues are disabled on this repository, so items 2-4 are tracked in that QUEUED
mission stub, which the command post surfaces at every intake.

Previous AAR action items: none found for this repo (`docs/aar/` is new here).
