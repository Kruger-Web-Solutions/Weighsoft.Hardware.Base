# Sprint template — RhynoSprintPlanCreate

Copy into the product repo as:

`docs/superpowers/plans/YYYY-MM-DD-<short>-sprint.md`

```markdown
# Sprint: <name>

**Created:** YYYY-MM-DD  
**Repo:** <folder name>  
**Branch:** <branch>  
**Skill:** RhynoSprintPlanCreate  
**Architecture plan:** <path>  
**Status:** draft | approved | in_build | closed

## Goal

One sentence.

## Data audit

| Check | Expected | Found | Result |
|-------|----------|-------|--------|
| Repo | … | … | PASS/FAIL |
| Branch | … | … | PASS/FAIL |
| Product option | … | … | PASS/FAIL |
| Caps | … | … | PASS/FAIL |
| Dropped features | … | … | PASS/FAIL |
| Waiting honest | … | … | PASS/FAIL |
| Wrong-repo items | none | … | PASS/FAIL |

## Phases (build order)

### Phase 1 — <name>
- RT ids: …
- Scope: …
- Acceptance: …
- Depends on: —

### Phase 2 — …
…

## Out of scope

- …

## Improvements (KPI — required)

Seed before build. Agents add rows when they find more.

| KPI id | Improvement | Source RT / finding | Status | Sprint |
|--------|-------------|---------------------|--------|--------|
| KPI-001 | … | … | open | this |

## Pre-build gate

- [ ] Data audit all PASS
- [ ] Plan matches decisions
- [ ] rhynoTodoList planned for this repo
- [ ] Improvements section seeded
- [ ] Jurien approved / said build
- [ ] No other-repo work

## Jurien decisions needed

1. …
```
