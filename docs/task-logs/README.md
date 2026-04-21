# Task logs and decision records

When you tell the agent to **follow the rules** (project + documentation workflow), the agent must:

1. **Create** a task document under `docs/task-logs/` if none exists for the current workstream, or **update** the existing one for that task.
2. **Maintain** a **roadmap** and a **decision log** for the duration of the task—not only at the end.
3. Treat the log as **draft** until you **explicitly sign off** on testing and outcomes.

## What belongs in a task document

- **Roadmap:** goals, phases, dependencies, open questions.
- **Decision log (every meaningful attempt):**
  - What was tried  
  - Why  
  - Outcome  
  - Why it failed or was rejected (if applicable)  
  - What was learned  
- **Proposed / current solution (not “final” before sign-off):**
  - Why it was selected  
  - Tradeoffs  
  - Risks  
  - Validation evidence (builds, tests, manual checks)—mark gaps as **pending**
- **Reverts:** document what was reverted and why.

## What must NOT go here vs official docs

- **Task logs** are working documents: DRAFT / IN PROGRESS until sign-off.
- **Do not** present a solution as **final** in the task log before sign-off.
- **Do not** update **official, permanent, or user-facing** documentation (e.g. `CHANGELOG.md`, `docs/API-REFERENCE.md`, product README sections meant for customers) **until** testing is complete **and** you explicitly sign off—unless you explicitly ask otherwise.

## Active task files

See filenames matching `TASK-*.md` in this folder. One file per concurrent major workstream is typical; the agent updates the file that matches the task under discussion.
