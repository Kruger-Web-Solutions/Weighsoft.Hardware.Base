# Mission stub: W-Soft bench build guard

STATUS: QUEUED

Filed by `docs/aar/aar-2026-10-31-wsoft-two-hour-build.md` (actions 2-4). Issues are
disabled on this repository, so this stub is the tracking record.

| # | Action | Owner | Due | Done when |
|---|---|---|---|---|
| A2 | `scripts/wsoft-build.ps1`: refuse a second `platformio run` in the same folder; print FULL or INCREMENTAL before compiling (PlatformIO checksum inputs: source file list under include/src/lib and platformio.ini, against `.pio/build/<env>/project.checksum`); run detached with its own log so an agent restart cannot kill it | Claude (laptop session) | 2026-11-03 | two builds started in one folder: the second is refused (proved able to fail); the FULL/INCREMENTAL line matches the compile count on the next two builds |
| A3 | Agents give a build estimate only after checking for a full rebuild; status lines say "file N of about M" | Claude (laptop session) | 2026-11-01 | rule in the bench guide's agent notes; the next two agent build reports quote FULL/INCREMENTAL with a file count |
| A4 | Measure one full build with Windows Defender exclusions for `C:\Users\User\.platformio` and the project `.pio` folder (security setting: the owner applies it) | Jurien | 2026-11-04 | time recorded in this file; exclusion kept only if it helps |
