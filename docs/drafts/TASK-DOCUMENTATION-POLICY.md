# Task documentation policy (“follow the rules”)

**Applies when:** the product owner writes **“follow the rules”** (or clearly invokes this same workflow).  
**Document type:** policy (draft folder; not a substitute for signed-off release docs).  
**Status:** active policy — last updated 2026-04-20.

---

## Non-negotiable requirements

1. **Roadmap + decision log** for the task shall exist and be kept current (default location: `docs/drafts/DECISION-LOG-<task-id>.md`; see [README.md](README.md)).
2. **Record all meaningful attempted solutions**, including **failed** and **reverted** ones. Omitting dead ends is not allowed if they informed the path.
3. **For each attempt**, document:
   - What was tried  
   - Why it was tried  
   - Outcome  
   - Why it failed or was rejected (if applicable)  
   - What was learned  
4. **For the solution that is kept as the working candidate** (not “final” until sign-off), document:
   - Why it works (mechanism / reasoning)  
   - Why it was selected over alternatives  
   - Tradeoffs  
   - Risks  
   - Validation evidence available at that time (build, bench test, production observation, etc.)  
5. If work is **still being tested** or not signed off, the task document must say **DRAFT** / **IN PROGRESS** prominently.
6. **Do not** update **official, permanent, or user-facing** documentation for the purpose of closing the task until testing is complete **and** the product owner **explicitly signs off**. Routine factual corrections requested separately are outside this gate but should still be proportionate.
7. **Never** present a solution as **final** before sign-off.
8. If **no** task document exists for the current task, **create one automatically** on first “follow the rules” invocation for that task.
9. **Update throughout** the task, not only at the end.
10. Completeness bar: **another developer can reconstruct the full decision path** from the task documentation.

---

## Agent checklist (each “follow the rules” touch)

- [ ] Identify the **active task** (or split if multiple parallel tasks need separate logs).  
- [ ] Open or create the matching `DECISION-LOG-*.md`.  
- [ ] Update roadmap table / phases if scope shifted.  
- [ ] Append decision entries for any new attempts since the last update.  
- [ ] Refresh validation evidence and risks.  
- [ ] Append a row to the log’s **changelog** section (who/when/what).  
- [ ] Confirm no premature “final” language; confirm official `docs/` unchanged unless explicitly out of scope and approved.

---

## Sign-off

Until the product owner provides **explicit written sign-off** for the task:

- Treat implementation as **candidate**.  
- Keep promotion to `docs/*.md` (and similar) **blocked** unless a separate, explicit request overrides.

**Sign-off template (copy into task log when done):**

> Task \<name\>: signed off by \<name\> on \<date\>. Official documentation may be updated: \<yes/no, which files\>.
