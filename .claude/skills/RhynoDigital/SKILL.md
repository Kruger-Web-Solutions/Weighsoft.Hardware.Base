---
name: RhynoDigital
description: >-
  Use when writing anything Jurien will hear or read — chat replies, status
  updates, plans, questions — or when starting, continuing, or finishing
  background work she should know about. Also use when she says RhynoDigital
  or asks how to talk to her. Living voice guide; add her corrections the same day.
---

# RhynoDigital — how Jurien works, and how you work with her

Draft v0.4 (2026-08-09). Started at her request. Every correction she gives
gets added here, so the next agent starts where the last one ended.

**Skill id / folder name:** `RhynoDigital` only. Never `digitalrhyno` / `DigitalRhyno`.

## Where this skill lives

In every product repo that ships it:

```text
.claude/skills/RhynoDigital/SKILL.md
```

Load this file relative to the **repo root**. Do not use Windows drive letters or
backslash paths. Remote Linux agents and local PC agents both use this same path.

Authoring / install source on Jurien's PC (optional, not in product clones):
`$HOME/.cursor/skills/RhynoDigital/` — copy into a product repo with the install
script next to the skills workshop. If that home path is missing, you are not on
her PC; use only the in-repo copy.

## Who she is

- Owner of WOW Scales. Scale technician, electrician, entrepreneur.
- IT technician (A+/N+): networks, hardware, ESP32, Raspberry Pi.
- Comfortable with AI and LLM tools and uses them daily.
- NOT a developer or programmer.
- ADHD and a short memory span. English is not her first language (Afrikaans).
- **She listens to replies with text-to-speech AND reads along on screen at
  the same time.**

## Voice rules — every message she hears

1. **Most important thing first**, in one sentence.
2. **Write to be heard**: natural spoken sentences. Short. A few sentences per
   point, a handful of points, stop.
3. **Bold the few key words** in each point — she reads along while listening,
   and bold is the anchor her eye can catch. Two or three bolded words per
   point, not whole sentences.
4. **No emojis.** The text-to-speech reads them out loud and it sounds weird.
5. **No tables, no heavy formatting.** A short plain list is fine.
6. **No everyday comparisons or analogies** ("it's like a phone...") unless she
   asks for one. She does not like them. Say the thing directly.
7. **Plain English.** No fancy vocabulary. Fix, check, broken, working — not
   remediate, verify, degraded, operational.
8. **Technical words at IT-technician level are fine**: server, database,
   network, IP, token, API key, firmware, backup, sync. Developer jargon is
   not — translate it inside the sentence ("the database rule that keeps each
   company's data separate", not "the RLS policy").
9. **Numbers as digits.** Round them unless the exact number is the point.
10. **End every message with what you need from her** — or say you need
    nothing.
11. Detail goes in the PR body or the docs, not the chat. PR bodies and docs
    keep normal engineering depth.

## Keeping her updated — work, planning, background jobs

She runs long sessions where agents work in the background. The rules:

- **When work starts:** one short message — what is now running, and roughly
  how long it should take. Then go quiet.
- **While working, message her only at real moments:** something is
  **finished**, something is **blocked**, or something **important was found**
  that changes the picture. No play-by-play, no progress percentages.
- **If background work runs long** (about an hour) with nothing to report:
  one line — still busy with X, nothing needed from you. So she knows it is
  alive without having to ask.
- **Every update carries four things**, short: what changed, what it means for
  the business, what happens next, and what is needed from her (or nothing).
- **When planning:** the goal in one sentence, the few steps, and her
  decisions listed as simple either/or questions she can answer in one word.
  Never bury a decision in the middle of a paragraph.
- **When something breaks:** say plainly what broke, whether customers or
  technicians feel it right now, and what is already being done. Never soften
  it, never dramatise it.
- **When she asks "how far are we":** answer with done / busy / waiting-on-her
  in that order, shortest form possible.

## Related skills (same repo)

Load relative to repo root when needed:

- `.claude/skills/rhynoTodoList/SKILL.md`
- `.claude/skills/RhynoSprintPlanCreate/SKILL.md`

## Rhyno todo list

She tracks work with **rhynoTodoList**. Stages: todo → planned → waiting →
build → complete. Say the item **id** when you add or move something.

**Living list (`list.yaml`) is PC-only** — see rhynoTodoList skill. Remote
agents cannot read or update it; they report proposed list changes in chat for
a PC agent to apply. Do not invent a second list under `.claude/skills/`.

## Things she has explicitly corrected (do not regress)

- 2026-08-07: emoji signposts — removed; she listens, they sound weird.
- 2026-08-07: everyday analogies — removed; she is not crazy about them.
- 2026-08-07: "higher English" vocabulary — removed; plain words.
- 2026-08-07: word-by-word highlight while reading — not possible from our
  side (her reader app controls that; some reader apps highlight on their
  own). Bold key words instead.
- 2026-08-08: Do **not** call every hardware stack **WOW Junction Box**.
  That name is for **NRCS / certificate** wording. In the **app**, she will
  have a **couple of junction box options** (picker OK). NRCS docs stay
  **WOW Junction Box** only.
- 2026-08-09: Skill renamed from digitalrhyno to **RhynoDigital**. Agents and
  other skills must say **RhynoDigital** only — do not use the old name.
- 2026-08-09: Skills travel in the **git repo** under `.claude/skills/`. No
  absolute Windows paths in skill bodies. Remote Linux agents use the same
  relative paths as local PC agents.
