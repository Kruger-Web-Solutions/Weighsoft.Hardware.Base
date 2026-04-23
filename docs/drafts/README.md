# Draft task documentation (`docs/drafts/`)

This folder holds **working** roadmaps and decision logs. Content here is **draft** until the product owner **explicitly signs off** on the underlying task and any promotion to permanent documentation.

## When the instruction is: “follow the rules”

That phrase triggers a **mandatory** workflow for the active task:

1. **Create** a task roadmap + decision log here if none exists for that task (naming: `DECISION-LOG-<short-task-id>.md`).
2. **Maintain** it throughout the task: every meaningful attempt (including failed and reverted work) gets a structured entry.
3. **Per attempt**, record: what was tried, why, outcome, why it failed or was rejected (if applicable), what was learned.
4. **For the retained / candidate solution** (never labelled “final” before sign-off): why it was selected, tradeoffs, risks, and validation evidence to date.
5. **Status:** mark work as **DRAFT** / **IN PROGRESS** while testing or review is incomplete.
6. **Official docs:** do **not** update permanent or user-facing documentation under `docs/` (e.g. `API-REFERENCE.md`, `CONFIGURATION.md`) or root `README.md` for *task closure* until testing is complete **and** the owner signs off—unless a change is explicitly requested outside that gate (e.g. factual bugfix in README). When in doubt, use this `drafts/` folder only.
7. **Never** present a solution as **final** before sign-off; use language like *candidate*, *merged in working tree*, *pending validation*.
8. **Goal:** another developer can reconstruct the full decision path from the task file(s) alone.

Canonical policy detail: [TASK-DOCUMENTATION-POLICY.md](TASK-DOCUMENTATION-POLICY.md)

## Active task logs

| Document | Scope | Status |
|----------|--------|--------|
| [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) | Serial + scale ingress hardening (Instance 1), related session notes | `IN PROGRESS` |
| [DECISION-LOG-wifi-sta-connectivity.md](DECISION-LOG-wifi-sta-connectivity.md) | STA WiFi not updating after factory credential change | `IN PROGRESS` |
| [DECISION-LOG-weight-forwarder-device-id.md](DECISION-LOG-weight-forwarder-device-id.md) | Weight Forwarder: auto `device_id` in payloads + read-only UI | `IN PROGRESS` |

Add new rows when new tasks get their own logs.
