# Integration Branch Workflow

## Overview

This repository uses `master` as the shared framework baseline and keeps major hardware integrations on their own long-lived branches.

Integration branches the docs point to (they exist on GitHub as of 2026-09-24; whether each is still in active use is not recorded):

- `RelayBoardEspBuildIn` - relay board and Live Weight integration baseline
- `SerialReaderWriter` - serial Reader / Writer / Diagnostics baseline; also holds the TFT screen (including the 3.5 inch `esp32dev_tft` board), the 16x2 character LCD (`lcddisplay`) and remote-weight options
- `weighingboard` - load-cell / weighing integration baseline
- `display` - the original 16x2 LCD code, before it was ported to `SerialReaderWriter` as `lcddisplay`

There is no `serial2` or `display35` branch. Serial work starts from `SerialReaderWriter`; 3.5 inch display work is the `esp32dev_tft` board on `SerialReaderWriter`; new 16x2 LCD work is `lcddisplay` on `SerialReaderWriter`. `serialReader` and `ALLinOneEspSerialdevice` are fully inside `SerialReaderWriter`. `serial`, `serialWriter` and `espReadWriteRHYNO` hold work that `SerialReaderWriter` does not have (for example WiFi fixes on `espReadWriteRHYNO`); they are not merged and their status is not decided. See [../ROADMAP.md](../ROADMAP.md) for the status of each line and the full branch list.

## Working Rules

1. Do not implement directly on `master`.
2. Do not implement directly on `RelayBoardEspBuildIn`, `SerialReaderWriter`, `weighingboard`, or `display`.
3. Choose the branch that already matches the integration you want to extend.
4. Create a separate working branch from that base branch for each task.
5. Merge the task branch back into the matching integration branch when the work is complete.

## Which Branch To Start From

Start from `master` when:

- You are changing shared framework code used by all integrations
- You are updating repo-wide onboarding or architecture documentation
- You are creating a brand-new integration baseline

Start from an integration branch when:

- You are extending an existing hardware integration
- You need existing code, screens, wiring, or protocol behavior from that integration
- The work is specific to one board or product variant

## Recommended Flow

### Shared framework or repo docs

```powershell
git switch master
git pull
git switch -c docs/repo-workflow
```

### Integration-specific work

```powershell
git switch weighingboard
git pull
git switch -c feature/weighingboard-calibration-audit
```

Use the same pattern for `RelayBoardEspBuildIn`, `SerialReaderWriter`, or `display`.

## Documentation Expectations

Update `master` when the change affects the whole repository:

- `README.md`
- `docs/README.md`
- `docs/INTEGRATION-WORKFLOW.md`
- shared architecture or configuration docs

Update the matching integration branch when the change is integration-specific:

- wiring and hardware notes
- REST, WebSocket, BLE, or MQTT details for that integration
- UI behavior and screenshots
- board-specific troubleshooting

## Branch Naming Suggestions

Examples:

- `docs/repo-workflow`
- `feature/display-brightness-controls`
- `feature/weighingboard-stability-fix`
- `fix/serialreaderwriter-buffering`

Keep the branch name tied to the target integration when the work is not shared.

## Before You Start

Checklist:

- Confirm which integration you are targeting
- Confirm with the user that they are happy to work on that branch before implementation starts
- Switch to the correct base branch
- Create a separate working branch
- Verify the docs you need are on that branch
- Keep repo-wide docs in `master`

## Cursor Rule Expectation

Cursor should use this document at the start of a new chat or plan to propose the correct branch and ask the user to confirm that branch before implementation work begins.

## Related Documents

- [../README.md](../README.md)
- [README.md](README.md)
- [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md)
- [DEVICE-TEMPLATE-GUIDE.md](DEVICE-TEMPLATE-GUIDE.md)
