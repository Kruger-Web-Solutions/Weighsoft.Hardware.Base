# Integration Branch Workflow

## Overview

This repository uses `master` as the shared framework baseline and keeps major hardware integrations on their own long-lived branches.

Current integration branches:

- `serial2` - serial integration baseline
- `display` - display integration baseline
- `weighingboard` - load-cell / weighing integration baseline
- `display35` - 3.5 inch display integration baseline

The older `serial` branch may still exist as historical reference, but new serial implementation work should start from `serial2`.

## Working Rules

1. Do not implement directly on `master`.
2. Do not implement directly on `serial2`, `display`, `weighingboard`, or `display35`.
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

Use the same pattern for `serial2`, `display`, or `display35`.

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
- `fix/serial2-buffering`

Keep the branch name tied to the target integration when the work is not shared.

## Before You Start

Checklist:

- Confirm which integration you are targeting
- Switch to the correct base branch
- Create a separate working branch
- Verify the docs you need are on that branch
- Keep repo-wide docs in `master`

## Related Documents

- [../README.md](../README.md)
- [README.md](README.md)
- [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md)
- [DEVICE-TEMPLATE-GUIDE.md](DEVICE-TEMPLATE-GUIDE.md)
