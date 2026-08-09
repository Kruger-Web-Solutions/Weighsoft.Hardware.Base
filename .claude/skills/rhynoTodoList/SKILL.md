---
name: rhynoTodoList
description: >-
  Use when Jurien says rhynoTodoList, "todo list", "add it to the list",
  "sprint plan", "what's on the list", or when starting / finishing Weighsoft
  work that should be tracked. Always check list access mode before planning or
  building; agents must surface open work and show what they added or proposed.
---

# rhynoTodoList

Living work list for Jurien. Speak with **RhynoDigital** voice. Not a git-only
backlog — a stage tracker with people, times, history, and **repo ownership**.

## Where this skill lives

```text
.claude/skills/rhynoTodoList/SKILL.md
```

Relative to the **repo root**. No drive letters. No backslash paths.

## Living data decision (do not guess)

| File | Policy | Location |
|------|--------|----------|
| `list.yaml` + `LIST.md` | **(B) PC-only** | Outside git. See below. |
| Sprint plans | **(A) in repo** | `docs/superpowers/plans/` |
| `kpi.yaml` | **(A) in repo** | `.claude/skills/RhynoSprintPlanCreate/kpi.yaml` |

**Do not** put `list.yaml` or `LIST.md` under `.claude/skills/`. One source of
truth for the living list; it is **not** committed to product repos.

### PC-only list path (local agents on Jurien's machine)

When present, edit only:

```text
$HOME/.cursor/skills/rhynoTodoList/list.yaml
$HOME/.cursor/skills/rhynoTodoList/LIST.md
```

Use forward slashes. Expand `$HOME` for the OS (Windows PC or WSL on that PC).
If these files exist → **LOCAL LIST MODE**: read/update/regenerate as below.

### Remote / no-list mode (cloud Linux, fresh container, no PC home)

If `$HOME/.cursor/skills/rhynoTodoList/list.yaml` is **missing** (normal for
Claude Code web / remote):

1. Say plainly: **list.yaml is PC-only — this agent cannot read or update it.**
2. Do **not** invent paths, do **not** create a list under `.claude/skills/`.
3. **Fallback:** report status and any proposed RT adds/moves/completes in chat
   (id if known, title, repo, stage, note). A **PC agent** applies them to
   `list.yaml` later.
4. Continue the product work in-repo (code, docs, kpi.yaml, sprint plans).

## Repo id

`repo` = the **git folder name of the current workspace** (basename of the repo
root). Do not invent short aliases (`weighsoft-hw`, `wow-djb`, …).

Examples (not an allowlist — any real workspace folder name is valid):

- `Weighsoft.Hardware.Base`
- `RS485-CanHatPi5DJB-W1X`
- `cal-certificate`
- `cursor-skills` / `Weighsoft.Skills` (skills workshop)

**Hard rule:** Only **add / move / plan / build** items for the **current**
`repo` unless Jurien names another repo. You may mention other repos briefly.

**Hard rule:** When regenerating `LIST.md` in LOCAL LIST MODE, group by repo.
Never dump mixed Active/Planned from two repos into one unlabelled pile.

## Stages (strict order)

1. **todo** — captured, not scheduled
2. **planned** — in a sprint / plan, not started
3. **waiting** — blocked on human, hardware, password, decision, flash mode, etc.
4. **build** — actively being implemented
5. **complete** — done; keep on list for history (never delete)

Optional label: `later` = park in **todo** until Jurien says to plan it.

## Item schema (`list.yaml`)

```yaml
items:
  - id: RT-001                    # never reuse (global across repos)
    title: Short plain title
    detail: One or two sentences
    repo: Weighsoft.Hardware.Base  # REQUIRED — git folder name
    project: Weighsoft.Hardware.Base
    branch: RelayBoardEspBuildIn       # if known
    stage: todo                        # todo|planned|waiting|build|complete
    later: false
    added_by: agent|jurien|name
    added_at: 2026-08-09T14:00:00+02:00
    stage_entered_at: 2026-08-09T14:00:00+02:00
    history:
      - stage: todo
        entered_at: 2026-08-09T14:00:00+02:00
        left_at: null
        by: agent
        note: why / how
    completed_at: null
    completed_by: null
    how_done: null
```

**Duration in a stage** = `left_at - entered_at` (or now - entered_at if still
in stage). When moving: close the open history row, append a new one, update
`stage` + `stage_entered_at`.

## Hard rules for every agent

1. Detect LOCAL LIST MODE vs remote fallback **before** claiming list status.
2. In LOCAL LIST MODE: always read `list.yaml` at the start of tracked work;
   filter to current `repo`.
3. If Jurien says **"add it to the list"** / **"for later"**: LOCAL → write
   todo/`later: true`; REMOTE → propose the item in chat for PC apply.
4. Agents **must** surface unfinished work they discover (write or propose).
5. Start build → **build**. Blocked → **waiting** + note. Done → **complete**
   with `how_done` (LOCAL write / REMOTE propose).
6. Before a sprint plan → move agreed items **todo → planned** for that repo
   only (LOCAL), or list the moves for PC apply (REMOTE).
7. Speak with **RhynoDigital** voice. Detail stays in list files or chat
   proposals.
8. LOCAL: after every list change → rewrite `LIST.md` from `list.yaml`.
9. Never delete **complete** items.
10. Never rewrite another session's repo items "to clean the list" without her ask.

## Phrases → actions

| Jurien says | Agent does |
|-------------|------------|
| add it to the list / for later | LOCAL: new todo/`later`. REMOTE: propose in chat |
| plan a sprint / RhynoSprintPlanCreate | load **RhynoSprintPlanCreate**; then LOCAL planned / REMOTE propose |
| start / build X | stage=build (same repo) or propose |
| waiting on me / blocked | stage=waiting + note or propose |
| done / complete | stage=complete + how/when/who or propose |
| what's on the list / show todo | LOCAL: read list. REMOTE: say PC-only + what you know from this chat/sprint docs |
| rhynoTodoList | load this skill + status by mode |

## How to add (LOCAL LIST MODE)

1. Read `list.yaml`
2. Workspace folder name → `repo`
3. Next id = max existing `RT-NNN` + 1 (global)
4. Append item with `repo`, `added_by`, `added_at` (ISO +02:00 in ZA)
5. History first row = current stage
6. Regenerate `LIST.md` (grouped by repo)
7. Tell Jurien: **id**, **repo**, title, stage, added_by, added_at

## How to move stage (LOCAL LIST MODE)

1. Confirm item `repo` matches the chat / her ask
2. Close current history row (`left_at` = now)
3. Append new history row
4. Set `stage`, `stage_entered_at`
5. If complete → fill `completed_*` + `how_done`
6. Regenerate `LIST.md`
7. Tell Jurien what moved and how long it sat in the old stage

## LIST.md format (TTS-friendly)

1. Header with `updated_at`
2. For **each repo** (active first): Active (build + waiting), Planned, Todo / later, Complete (newest first, short)
3. No emoji. Plain lists. Every line under a repo heading.

## Sprint flow

1. LOCAL: read list. REMOTE: use sprint plan docs + chat; say list is PC-only.
2. Propose sprint from this repo's work
3. On approval → planned (LOCAL write / REMOTE propose for PC)
4. Keep stages in sync as work proceeds
5. End of sprint → what completed and what remains for that repo

## Do not

- Invent completions
- Instruct a read of a Windows-only absolute path
- Create a second `list.yaml` under `.claude/skills/`
- Mix two repos in one unlabelled Active block
- Put secrets (WiFi passwords) in the list — say "password on device" only
