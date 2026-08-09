---
name: RhynoSprintPlanCreate
description: >-
  Use when Jurien says RhynoSprintPlanCreate, "create a sprint plan",
  "sprint plan", "plan the sprint", or before starting Option A / rhynoTodoList
  planned work. Complements rhynoTodoList; does not replace weighsoft-plan-execute
  for multi-PR delivery.
---

# RhynoSprintPlanCreate

Creates a **sprint plan** Jurien can approve, then agents must **pass the gate**
before build. Speaks to Jurien with **RhynoDigital**. Tracks **improvements** for **KPI**.

## Where this skill lives

```text
.claude/skills/RhynoSprintPlanCreate/SKILL.md
.claude/skills/RhynoSprintPlanCreate/sprint-template.md
.claude/skills/RhynoSprintPlanCreate/kpi.yaml
```

All paths relative to the **repo root**. No drive letters. No backslash paths.

## Related skills (load when needed)

| Skill | Path |
|-------|------|
| `rhynoTodoList` | `.claude/skills/rhynoTodoList/SKILL.md` |
| `RhynoDigital` | `.claude/skills/RhynoDigital/SKILL.md` |

## Triggers

- `RhynoSprintPlanCreate`
- create / plan a sprint
- before build of a planned Option A (or similar) backlog

## Hard rules

1. **One repo per sprint.** `repo` = git folder name of the current workspace. Never mix two product repos in one sprint.
2. **No build until GATE PASS.** Coding, flash, PR open for sprint work = only after gate checklist is green and Jurien said go (or she already approved the written plan).
3. **Check data first.** Caps, branch, dropped features must match code + plan docs. Fix or flag wrong data **before** build.
4. **Improvements are mandatory.** Every sprint must have an Improvements section. Agents **must** add improvements they find. Log them in in-repo `kpi.yaml`. Do not silently skip.
5. **Waiting stays waiting.** Field checks blocked on Jurien/hardware stay `waiting` — do not fake complete.
6. **Later stays later** unless she pulls it into the sprint.
7. After plan write → move sprint items **todo → planned** in rhynoTodoList when in **LOCAL LIST MODE**; on remote, propose those moves in chat for a PC agent.

## Living data

| File | Policy | Path |
|------|--------|------|
| Sprint plan | **(A) in repo** | `docs/superpowers/plans/YYYY-MM-DD-<repo-short>-sprint.md` |
| KPI log | **(A) in repo** | `.claude/skills/RhynoSprintPlanCreate/kpi.yaml` |
| Todo list | **(B) PC-only** | See rhynoTodoList — not readable on remote |

## Workflow (do in order)

### Step 0 — Identity

1. Workspace folder name → `repo`
2. `git branch --show-current` → `branch`
3. Load rhynoTodoList skill. If LOCAL LIST MODE: read `list.yaml` + `LIST.md` filtered to this `repo`. If remote: state that the living list is PC-only and continue using sprint docs + chat.

### Step 1 — Data audit (must pass)

Compare these sources; write **PASS / FAIL** per row in the sprint plan:

| Check | Sources |
|-------|---------|
| Repo + branch | workspace, git, RT items / chat |
| Product decisions | architecture / options plan in `docs/` |
| Dropped features gone from code | e.g. buzzer stripped |
| Caps | plan + RT detail when known |
| Waiting items honest | still need human/hardware? |
| Wrong-repo items | none in this sprint |
| Dirty tree known | list uncommitted work that affects sprint |
| List access | LOCAL LIST MODE or remote fallback stated |

On **FAIL**: fix list/docs if safe, or block sprint and ask Jurien. **Do not build.**

### Step 2 — Plan audit

Read the architecture / options plan for this work. Confirm sprint scope matches (Option A vs B, caps, deferred). Link the plan path in the sprint doc.

### Step 3 — Build the sprint plan file

Write:

```text
docs/superpowers/plans/YYYY-MM-DD-<repo-short>-sprint.md
```

Use the template in [sprint-template.md](sprint-template.md).

Include: goal, repo / branch / dates, data audit, ordered phases with RT ids + acceptance, out of scope, Improvements backlog, KPI pointer to `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`, gate checklist, what Jurien must approve.

### Step 4 — Pre-build gate (before any sprint coding)

```text
GATE:
- [ ] Data audit all PASS (or FAIL fixed + re-audited)
- [ ] Plan doc matches Option / caps / dropped features
- [ ] Sprint items planned in rhynoTodoList for this repo (LOCAL) OR proposed for PC apply (remote) and Jurien accepts that
- [ ] Improvements section exists (at least seed KPIs)
- [ ] Jurien approved sprint (or explicit "build" / "go")
- [ ] No work from other repos in this sprint
```

Only when all checked → agents may move first phase to **build**.

### Step 5 — During sprint (every agent)

1. Move RT **planned → build** when starting; **build → complete** when done with `how_done` (LOCAL write / REMOTE propose).
2. On block → **waiting** + note.
3. **When you find an improvement** (required):
   - Add to sprint plan Improvements table (`KPI-###`)
   - Append row to `.claude/skills/RhynoSprintPlanCreate/kpi.yaml`
   - Optionally propose/add RT on rhynoTodoList (`later: true` if not in this sprint)
4. Do not expand scope without Jurien OK — park as improvement / later RT.

### Step 6 — Close sprint

1. Mark phases done / slipped in sprint doc
2. KPI: set `status` shipped / carried / dropped + `closed_at` / `shipped_at` as appropriate
3. Tell Jurien: completed RT count, improvements found, KPI snapshot, what still waiting (and whether list updates still need PC apply)

## KPI (improvements)

In-repo file (both local and remote may edit):

```text
.claude/skills/RhynoSprintPlanCreate/kpi.yaml
```

Counts:

- `improvements_logged` — found and written down
- `improvements_shipped` — done in a sprint
- `improvements_carried` — deferred with id
- Sprint completion % = complete RT / planned RT (waiting field-checks listed separate)

Agents **must** log improvements; zero improvements with a large sprint is a process fail unless she said "no KPI this sprint".

## Talk to Jurien

Use **RhynoDigital**: goal first, bold key words, no emoji/tables in chat (tables OK in the plan file). End with what you need (approve sprint / build / fix FAIL).

## Anti-patterns

- Building from chat memory without data audit
- Mixing other product repos into this sprint
- Completing field-check RTs without her test
- Shipping without logging obvious improvements
- Skipping gate because "small change"
- Instructing reads of absolute Windows paths that remote agents cannot open
